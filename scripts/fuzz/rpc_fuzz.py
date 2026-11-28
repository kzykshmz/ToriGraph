#!/usr/bin/env python3
"""Parallel JSON-RPC fuzzer for a (sanitizer-instrumented) ToriGraph build.

Each worker owns one headless ToriGraph process with an isolated HOME / XDG dirs,
drives it with test cases built from ``gen_corpus.py`` data and from the server's
own API schema (``app.describe_api``), and detects

* crashes  -- process exit / ASan / UBSan report on stderr,
* hangs    -- no answer within the per-call timeout,
* internal errors -- JSON-RPC error code -32603 or a non-JSON answer,
* sanitizer reports that did not kill the process (UBSan with recover).

A crashed instance is restarted and the run continues.  Every finding is written to
``OUT/findings/<n>.json`` with the full call sequence of the failing case, so it can be
replayed with ``--replay FILE``.

Usage::

    python scripts/fuzz/rpc_fuzz.py --exe build-asan/scidavis/ToriGraph \
        --corpus build-fuzz/corpus --out build-fuzz/run1 --workers 4 \
        --phases imports,projects,analysis,schema --budget 900
"""

from __future__ import annotations

import argparse
import glob
import json
import math
import multiprocessing as mp
import os
import random
import re
import shutil
import signal
import subprocess
import sys
import time
import traceback
from pathlib import Path
from typing import Any

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "python"))
from torigraph.client import _TcpTransport

SAN_MARK = re.compile(r"(ERROR: AddressSanitizer|runtime error:|ERROR: LeakSanitizer|SUMMARY: UndefinedBehaviorSanitizer|"
                      r"Assertion .* failed|ASSERT failure|terminate called|Segmentation fault)")
# Qt warnings that are interesting enough to report (e.g. index out of range in QList::at).
QT_MARK = re.compile(r"(ASSERT: |QList::at|index out of range|QVector::at|QString::at)")


# ---------------------------------------------------------------------------
# Process wrapper
# ---------------------------------------------------------------------------

class Dead(Exception):
    pass


class Hang(Exception):
    pass


class App:
    def __init__(self, exe: Path, workdir: Path, call_timeout: float):
        self.exe = Path(exe).resolve()
        self.workdir = workdir
        self.call_timeout = call_timeout
        self.proc: subprocess.Popen | None = None
        self.tr: _TcpTransport | None = None
        self.err_path = workdir / "stderr.log"
        self.err_offset = 0
        self.next_id = 1
        self.start()

    def start(self) -> None:
        self.stop()
        home = self.workdir / "home"
        shutil.rmtree(home, ignore_errors=True)
        for sub in ("", ".config", ".local/share", ".cache", "tmp"):
            (home / sub).mkdir(parents=True, exist_ok=True)
        (home / "tmp").chmod(0o700)
        env = os.environ.copy()
        env.update({
            "HOME": str(home),
            "XDG_CONFIG_HOME": str(home / ".config"),
            "XDG_DATA_HOME": str(home / ".local/share"),
            "XDG_CACHE_HOME": str(home / ".cache"),
            "XDG_RUNTIME_DIR": str(home / "tmp"),
            "TMPDIR": str(home / "tmp"),
            "QT_QPA_PLATFORM": "offscreen",
            "QT_OPENGL": "software",
            "LIBGL_ALWAYS_SOFTWARE": "1",
            "TORIGRAPH_NO_OLE_REGISTER": "1",
            "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1:abort_on_error=0:detect_stack_use_after_return=0:"
                            "allocator_may_return_null=1:handle_abort=1",
            "UBSAN_OPTIONS": "print_stacktrace=1:halt_on_error=1",
        })
        port_file = home / "tmp" / "port.txt"
        self.errf = open(self.err_path, "ab")
        self.err_offset = self.err_path.stat().st_size
        self.proc = subprocess.Popen(
            [str(self.exe), "--headless", "--rpc-port", "0", "--rpc-port-file", str(port_file)],
            stdin=subprocess.DEVNULL, stdout=self.errf, stderr=self.errf, env=env, cwd=str(home),
            start_new_session=True)
        deadline = time.monotonic() + 120
        ep = ""
        while time.monotonic() < deadline:
            if port_file.exists():
                ep = port_file.read_text().strip().splitlines()[0] if port_file.read_text().strip() else ""
                if ep:
                    break
            if self.proc.poll() is not None:
                raise RuntimeError(f"app died at startup: {self.stderr_tail()}")
            time.sleep(0.05)
        if not ep:
            raise RuntimeError("timeout waiting for rpc endpoint")
        port = int(ep.split("?")[0].rsplit(":", 1)[1])
        tok = None
        tdeadline = time.monotonic() + 30
        while tok is None and time.monotonic() < tdeadline:
            for f in glob.glob(str(home / "**" / f"tcp_{port}.token"), recursive=True, include_hidden=True):
                tok = Path(f).read_text().strip() or None
            if tok is None:
                time.sleep(0.05)
        self.tr = _TcpTransport("127.0.0.1", port, timeout=self.call_timeout, token=tok)
        self.err_offset = self.err_path.stat().st_size

    def stop(self) -> None:
        if self.tr:
            try:
                self.tr.close()
            except Exception:
                pass
            self.tr = None
        if self.proc and self.proc.poll() is None:
            try:
                os.killpg(self.proc.pid, signal.SIGKILL)
            except Exception:
                self.proc.kill()
            try:
                self.proc.wait(5)
            except Exception:
                pass
        self.proc = None

    def alive(self) -> bool:
        return self.proc is not None and self.proc.poll() is None

    def stderr_new(self) -> str:
        try:
            with open(self.err_path, "rb") as f:
                f.seek(self.err_offset)
                data = f.read()
            self.err_offset += len(data)
            return data.decode("utf-8", "replace")
        except OSError:
            return ""

    def stderr_tail(self, n: int = 12000) -> str:
        try:
            return self.err_path.read_bytes()[-n:].decode("utf-8", "replace")
        except OSError:
            return ""

    def raw(self, payload: bytes, timeout: float | None = None) -> dict:
        t = timeout or self.call_timeout
        try:
            line = self.tr.request_line(payload, timeout=t)
        except Exception as exc:
            time.sleep(0.3)
            if not self.alive():
                raise Dead(str(exc))
            if "Timed out" in str(exc):
                raise Hang(str(exc))
            raise Dead(str(exc))
        try:
            return json.loads(line.decode("utf-8"))
        except Exception:
            return {"__nonjson__": line[:2000].decode("utf-8", "replace")}

    def call(self, method: str, params: Any, timeout: float | None = None) -> dict:
        rid = self.next_id
        self.next_id += 1
        body = json.dumps({"jsonrpc": "2.0", "id": rid, "method": method, "params": params},
                          ensure_ascii=False, allow_nan=True).encode("utf-8", "surrogatepass")
        return self.raw(body, timeout)


# ---------------------------------------------------------------------------
# Value generation from the API schema
# ---------------------------------------------------------------------------

EDGE_INTS = [0, 1, -1, 2, 3, 7, 100, 2**31 - 1, -2**31, 2**31, 2**53, -2**53, 10**12]
EDGE_FLOATS = [0.0, -0.0, 1.0, -1.0, 0.5, 1e-300, -1e-300, 5e-324, 1e300, -1e300, 1e308,
               float("nan"), float("inf"), float("-inf"), 3.14159, 1e9, -1e9]
EDGE_STRS = ["", " ", "x", "Table1", "Graph1", "../../etc/passwd", "a" * 5000, "日本語", "\u0000",
             "%n%C", "$(rm -rf)", "Table1_1", "col(\"A\")", "1e5", "nan", "\ud800"]


def parse_schema(s: str) -> list[tuple[str, str, str, list[str]]]:
    """'a:int=1; b:str=''[x|y]' -> [(name, type, default, enum)]."""
    out = []
    for part in s.split(";"):
        part = part.strip()
        if not part or ":" not in part:
            continue
        name, rest = part.split(":", 1)
        enum: list[str] = []
        m = re.search(r"\[([^\]]*\|[^\]]*)\]\s*$", rest)
        if m:
            enum = [e for e in m.group(1).split("|") if e]
            rest = rest[: m.start()]
        typ, _, default = rest.partition("=")
        out.append((name.strip(), typ.strip(), default.strip(), enum))
    return out


class Gen:
    def __init__(self, rng: random.Random, ctx: dict):
        self.r = rng
        self.ctx = ctx  # names of existing tables / graphs / matrices

    def val(self, name: str, typ: str, enum: list[str]) -> Any:
        r = self.r
        if enum and r.random() < 0.8:
            return r.choice(enum)
        lname = name.lower()
        if lname in ("table", "source", "src_table", "dest_table") or lname.endswith("_table"):
            return r.choice(self.ctx.get("tables", ["Table1"]) + [r.choice(EDGE_STRS)])
        if lname in ("graph", "graph_name", "target") or lname.endswith("_graph"):
            return r.choice(self.ctx.get("graphs", ["Graph1"]) + [r.choice(EDGE_STRS)])
        if lname in ("matrix",):
            return r.choice(self.ctx.get("matrices", ["Matrix1"]) + [r.choice(EDGE_STRS)])
        if lname in ("path", "file", "filename", "out", "output"):
            return r.choice([str(self.ctx["tmp"] / f"f{r.randrange(1000)}.dat"), "", "/nonexistent/x/y.png",
                             str(self.ctx["tmp"]), "relative.png"])
        t = typ.split("|")[0].strip().lower()
        if r.random() < 0.08:
            return r.choice([None, [], {}, "", True, -1, float("nan")])
        if t.startswith("int"):
            return r.choice(EDGE_INTS) if r.random() < 0.6 else r.randrange(-5, 40)
        if t.startswith("float") or t.startswith("number"):
            return r.choice(EDGE_FLOATS) if r.random() < 0.6 else r.uniform(-10, 10)
        if t.startswith("bool"):
            return r.choice([True, False, 0, 1, "true"])
        if t.startswith("list"):
            n = r.choice([0, 1, 2, 3, 5, 50])
            if "float" in t:
                return [r.choice(EDGE_FLOATS + [r.uniform(-5, 5)] * 5) for _ in range(n)]
            if "str" in t:
                return [r.choice(EDGE_STRS) for _ in range(n)]
            return [r.choice([0, 1, 2, -1, "A", "B", 99]) for _ in range(n)]
        if t.startswith("dict"):
            return r.choice([{}, {"a": 1}, {"A": float("nan"), "B": [1, 2]}, {"x": {"y": None}}])
        if t.startswith("str"):
            return r.choice(EDGE_STRS + ["A", "B", "1", "2", "x", "y"])
        return r.choice([None, 0, "", [], 1.5])

    def params(self, schema: list, density: float = 0.6) -> dict:
        p = {}
        for name, typ, default, enum in schema:
            if self.r.random() < density:
                p[name] = self.val(name, typ, enum)
        return p


# ---------------------------------------------------------------------------
# Case builders.  A case is a list of (method, params, timeout_or_None).
# ---------------------------------------------------------------------------

def load_series(corpus: Path) -> dict[str, list]:
    out = {}
    for f in sorted((corpus / "series").glob("*.json")):
        vals = json.loads(f.read_text())
        out[f.stem] = [float(v) if isinstance(v, str) else v for v in vals]
    return out


def cases_imports(corpus: Path, rng: random.Random) -> list[tuple[str, list]]:
    cases = []
    for f in sorted((corpus / "csv").iterdir()):
        variants = [{}, {"separator": ";"}, {"header": False}, {"header": True, "units_row": 1},
                    {"skip_lines": 3}, {"separator": "\t", "with_comments": True}, {"numeric": False},
                    {"encoding": "shift_jis"}, {"skip_lines": 10**9}, {"separator": ""}]
        for v in variants[: 4 if f.stat().st_size > 1_000_000 else len(variants)]:
            name = f"T{rng.randrange(10**6)}"
            seq = [("app.import_csv", {"path": str(f), "name": name, **v}, 120),
                   ("table.get_info", {"table": name}, None),
                   ("table.column_stats", {"table": name, "col": 1}, None),
                   ("plot.xy", {"table": name, "style": "line_symbol"}, 60),
                   ("table.sort", {"table": name, "col": 0}, 60),
                   ("table.export_csv", {"table": name, "path": "@tmp/out.csv"}, 60),
                   ("app.undo", {}, None), ("app.redo", {}, None),
                   ("app.close_window", {"name": name}, None)]
            cases.append((f"csv:{f.name}:{json.dumps(v)}", seq))
    for f in sorted((corpus / "xlsx").iterdir()):
        for wb in (True, False):
            cases.append((f"xlsx:{f.name}:{wb}", [("app.import_xlsx", {"path": str(f), "workbook": wb}, 120),
                                                  ("app.list_windows", {}, None),
                                                  ("app.undo", {}, None)]))
    for f in sorted((corpus / "opj").iterdir()):
        cases.append((f"opj:{f.name}", [("app.import_opj", {"path": str(f)}, 120), ("app.list_windows", {}, None),
                                        ("app.new_project", {"force": True}, None)]))
    return cases


def cases_projects(corpus: Path, rng: random.Random) -> list[tuple[str, list]]:
    cases = []
    for f in sorted((corpus / "project").iterdir()):
        cases.append((f"project:{f.name}", [
            ("app.open_project", {"path": str(f), "force": True}, 120),
            ("app.list_windows", {}, None),
            ("@each_window", {}, None),
            ("app.save_project", {"path": "@tmp/resave.sciprj"}, 120),
            ("app.open_project", {"path": "@tmp/resave.sciprj", "force": True}, 120),
            ("app.undo", {}, None),
            ("app.new_project", {"force": True}, None)]))
    return cases


ANALYSIS_METHODS_HINT = ("signal.", "curvemath.", "analysis.", "stats.", "gadget.")


def cases_analysis(series: dict, api: dict, rng: random.Random, n_per: int) -> list[tuple[str, list]]:
    cases = []
    methods = [m for m in api if m.startswith(ANALYSIS_METHODS_HINT)]
    sx = list(series.items())
    for m in methods:
        schema = api[m]
        for k in range(n_per):
            xname, xs = rng.choice(sx)
            yname, ys = rng.choice(sx)
            n = min(len(xs), len(ys), 5000)
            if rng.random() < 0.3:
                xs = [i * 0.1 for i in range(len(ys))]
                xname = "index"
                n = min(len(ys), 5000)
            cases.append((f"analysis:{m}:{xname}/{yname}:{k}", [
                ("@setup_xy", {"x": xs[:n], "y": ys[:n]}, None),
                (m, {"@schema": schema, "@seed": rng.randrange(2**31)}, 60),
                ("app.list_windows", {}, None),
                ("app.undo", {}, None),
            ]))
    return cases


def cases_schema(api: dict, rng: random.Random, n_per: int) -> list[tuple[str, list]]:
    cases = []
    skip = {"app.check_updates", "app.grab_gui_diagnostic", "app.trigger_action"}
    for m, schema in api.items():
        if m in skip:
            continue
        for k in range(n_per):
            cases.append((f"schema:{m}:{k}", [
                ("@setup_full", {}, None),
                (m, {"@schema": schema, "@seed": rng.randrange(2**31)}, 60),
                (rng.choice(list(api)), {"@schema": api[rng.choice(list(api))], "@seed": rng.randrange(2**31)}, 60),
                ("app.undo", {}, None), ("app.redo", {}, None),
                ("app.list_windows", {}, None),
            ]))
    return cases


# ---------------------------------------------------------------------------
# Worker
# ---------------------------------------------------------------------------

def _resolve(params: Any, tmp: Path) -> Any:
    if isinstance(params, str) and params.startswith("@tmp/"):
        return str(tmp / params[5:])
    if isinstance(params, dict):
        return {k: _resolve(v, tmp) for k, v in params.items()}
    if isinstance(params, list):
        return [_resolve(v, tmp) for v in params]
    return params


def run_case(app: App, name: str, seq: list, tmp: Path, log: list) -> None:
    ctx = {"tables": [], "graphs": [], "matrices": [], "tmp": tmp}
    for method, params, timeout in seq:
        if method == "@setup_xy":
            app.call("app.new_project", {"force": True})
            r = app.call("table.create", {"rows": max(1, len(params["x"])), "cols": 2, "name": "Data"})
            log.append(("table.create", "...", _short(r)))
            for ci, key in ((0, "x"), (1, "y")):
                r = app.call("table.set_column_data", {"table": "Data", "col": ci, "data": params[key]})
                log.append(("table.set_column_data", {"col": ci, "n": len(params[key])}, _short(r)))
            r = app.call("plot.xy", {"table": "Data", "x_col": 0, "y_cols": [1], "graph_name": "G", "style": "line_symbol"}, 60)
            log.append(("plot.xy", {}, _short(r)))
            ctx["tables"] = ["Data"]
            ctx["graphs"] = ["G"] + ([r["result"].get("name")] if isinstance(r.get("result"), dict) and r["result"].get("name") else [])
            continue
        if method == "@setup_full":
            app.call("app.new_project", {"force": True})
            app.call("table.create", {"rows": 40, "cols": 4, "name": "Data"})
            for ci in range(4):
                app.call("table.set_column_data", {"table": "Data", "col": ci,
                                                   "data": [math.sin(i * 0.3 + ci) * (ci + 1) + ci for i in range(40)]})
            r = app.call("plot.xy", {"table": "Data", "x_col": 0, "y_cols": [1, 2], "graph_name": "G"}, 60)
            app.call("matrix.create", {"rows": 8, "cols": 8, "name": "M"})
            ctx.update(tables=["Data"], graphs=["G"], matrices=["M"])
            continue
        if method == "@each_window":
            r = app.call("app.list_windows", {})
            wins = (r.get("result") or {}).get("windows", []) if isinstance(r.get("result"), dict) else []
            for w in wins[:20]:
                wn = w.get("name") if isinstance(w, dict) else w
                for mm in ("app.window_info", "graph.get_info", "table.get_info", "matrix.get_info", "graph.inspect"):
                    rr = app.call(mm, {"name": wn, "graph": wn, "table": wn, "matrix": wn}, 60)
                    log.append((mm, wn, _short(rr)))
                    _check_resp(rr, mm)
                rr = app.call("graph.export", {"graph": wn, "path": str(tmp / "exp.png")}, 60)
                log.append(("graph.export", wn, _short(rr)))
                _check_resp(rr, "graph.export")
            continue
        if isinstance(params, dict) and "@schema" in params:
            g = Gen(random.Random(params["@seed"]), ctx)
            params = g.params(params["@schema"])
        params = _resolve(params, tmp)
        r = app.call(method, params, timeout)
        log.append((method, _short(params), _short(r)))
        _check_resp(r, method)


class Internal(Exception):
    pass


def _check_resp(r: dict, method: str) -> None:
    if "__nonjson__" in r:
        raise Internal(f"non-JSON answer to {method}: {r['__nonjson__'][:300]}")
    err = r.get("error")
    if isinstance(err, dict) and err.get("code") == -32603:
        raise Internal(f"internal error from {method}: {str(err.get('message'))[:500]}")


def _short(x: Any, n: int = 400) -> Any:
    s = json.dumps(x, ensure_ascii=False, default=str, allow_nan=True)
    return s if len(s) <= n else s[:n] + "..."


def worker(idx: int, cases: list, args_d: dict, q: mp.Queue) -> None:
    out = Path(args_d["out"])
    wd = out / f"w{idx}"
    wd.mkdir(parents=True, exist_ok=True)
    tmp = wd / "tmp"
    tmp.mkdir(exist_ok=True)
    app = App(Path(args_d["exe"]), wd, args_d["timeout"])
    deadline = time.monotonic() + args_d["budget"]
    done = 0
    for name, seq in cases:
        if time.monotonic() > deadline:
            break
        log: list = []
        kind = None
        detail = ""
        try:
            run_case(app, name, seq, tmp, log)
            alive = app.call("ping", {}, 30)
            if "result" not in alive:
                raise Internal(f"ping failed: {alive}")
        except Dead as e:
            kind, detail = "crash", str(e)
        except Hang as e:
            kind, detail = "hang", str(e)
        except Internal as e:
            kind, detail = "internal", str(e)
        except Exception:  # driver bug: keep going
            kind, detail = "driver", traceback.format_exc()[-1500:]
        err = app.stderr_new()
        san = [ln for ln in err.splitlines() if SAN_MARK.search(ln)]
        qt = [ln for ln in err.splitlines() if QT_MARK.search(ln)]
        if san and kind is None:
            kind = "sanitizer"
        if kind is None and qt:
            kind = "qt-warning"
        if kind:
            q.put({"worker": idx, "case": name, "kind": kind, "detail": detail[:3000],
                   "stderr": (err if kind != "qt-warning" else "\n".join(qt))[-20000:],
                   "log": log[-40:], "seq": _short(seq, 200000)})
            if kind in ("crash", "hang", "sanitizer", "driver") or not app.alive():
                try:
                    app.start()
                except Exception as e:
                    q.put({"worker": idx, "case": name, "kind": "restart-failed", "detail": str(e)})
                    return
        done += 1
        if done % 25 == 0:
            q.put({"progress": idx, "done": done, "total": len(cases)})
    app.stop()
    q.put({"finished": idx, "done": done})


def fetch_api(exe: Path, out: Path, timeout: float) -> dict:
    wd = out / "w_api"
    wd.mkdir(parents=True, exist_ok=True)
    app = App(exe, wd, timeout)
    try:
        r = app.call("app.describe_api", {})
    finally:
        app.stop()
    res = r.get("result", {})
    entries = res.get("methods") or res.get("api") or res
    api = {}
    if isinstance(entries, dict):
        entries = [dict(v, method=k) if isinstance(v, dict) else {"method": k, "params": v} for k, v in entries.items()]
    for e in entries if isinstance(entries, list) else []:
        if not isinstance(e, dict):
            continue
        m = e.get("method") or e.get("name")
        ps = e.get("params") or []
        if m and isinstance(ps, list):
            api[m] = [(str(p.get("name", "")), str(p.get("type", "")), json.dumps(p.get("default")),
                       [str(x) for x in (p.get("enum") or [])]) for p in ps if isinstance(p, dict)]
        elif m:
            api[m] = parse_schema(str(ps))
    (out / "api.json").write_text(json.dumps(api, indent=1, ensure_ascii=False))
    return api


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=str(REPO / "build-asan/scidavis/ToriGraph"))
    ap.add_argument("--corpus", default=str(REPO / "build-fuzz/corpus"))
    ap.add_argument("--out", default=str(REPO / "build-fuzz/run"))
    ap.add_argument("--workers", type=int, default=max(1, (os.cpu_count() or 2)))
    ap.add_argument("--phases", default="imports,projects,analysis,schema")
    ap.add_argument("--budget", type=float, default=1200, help="seconds per worker")
    ap.add_argument("--timeout", type=float, default=60)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--n-analysis", type=int, default=6)
    ap.add_argument("--n-schema", type=int, default=3)
    ap.add_argument("--filter", default="", help="regex on case name")
    args = ap.parse_args()
    args.out = str(Path(args.out).resolve())
    args.corpus = str(Path(args.corpus).resolve())
    args.exe = str(Path(args.exe).resolve())

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    (out / "findings").mkdir(exist_ok=True)
    rng = random.Random(args.seed)
    corpus = Path(args.corpus)
    phases = set(args.phases.split(","))
    api = fetch_api(Path(args.exe), out, args.timeout) if phases & {"analysis", "schema"} else {}
    print(f"api methods: {len(api)}", flush=True)
    cases: list = []
    if "imports" in phases:
        cases += cases_imports(corpus, rng)
    if "projects" in phases:
        cases += cases_projects(corpus, rng)
    if "analysis" in phases:
        cases += cases_analysis(load_series(corpus), api, rng, args.n_analysis)
    if "schema" in phases:
        cases += cases_schema(api, rng, args.n_schema)
    if args.filter:
        cases = [c for c in cases if re.search(args.filter, c[0])]
    rng.shuffle(cases)
    print(f"{len(cases)} cases, {args.workers} workers", flush=True)
    shards = [cases[i:: args.workers] for i in range(args.workers)]
    q: mp.Queue = mp.Queue()
    procs = [mp.Process(target=worker, args=(i, shards[i], vars(args), q)) for i in range(args.workers)]
    for p in procs:
        p.start()
    finished = 0
    nfind = 0
    summary: dict[str, int] = {}
    while finished < len(procs):
        try:
            msg = q.get(timeout=5)
        except Exception:
            if not any(p.is_alive() for p in procs):
                break
            continue
        if "finished" in msg:
            finished += 1
            print(f"worker {msg['finished']} finished ({msg['done']} cases)", flush=True)
        elif "progress" in msg:
            print(f"worker {msg['progress']}: {msg['done']}/{msg['total']}", flush=True)
        else:
            nfind += 1
            summary[msg["kind"]] = summary.get(msg["kind"], 0) + 1
            (out / "findings" / f"{nfind:04d}_{msg['kind']}.json").write_text(
                json.dumps(msg, indent=1, ensure_ascii=False, default=str))
            print(f"FINDING #{nfind} [{msg['kind']}] {msg['case']} :: {msg.get('detail','')[:160]}", flush=True)
    for p in procs:
        p.join(5)
    print("summary:", summary, flush=True)
    (out / "summary.json").write_text(json.dumps(summary))
    return 0


if __name__ == "__main__":
    sys.exit(main())
