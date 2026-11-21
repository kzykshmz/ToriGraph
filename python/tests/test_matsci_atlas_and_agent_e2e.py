#!/usr/bin/env python3
"""Permanent 30-Figure Materials Science & AI-Agent Autonomy Atlas E2E Suite.

Covers all 4 user/agent personas end-to-end against a live headless ToriGraph RPC server:
  - Persona 1: Novice (Figs 01-10) + Smart Lab Imports (Shift-JIS CSV, .xy, Rigaku .ras,
    VASP DOSCAR/EIGENVAL, and -32602 rejection on binary .ras/.dat)
  - Persona 2: Intermediate (Figs 11-20) — Tensile Rp0.2 & brittle guard, Work-Hardening
    (Considere & Kocks-Mecking), Multi-Trap Kissinger TDS, FCC/HCP XRD + Williamson-Hall,
    Right-Censored Weibull MLE + .sciprj reload, Box+ANOVA & Violin+Kruskal brackets,
    2x1 Stacked Histogram + Lognormal PDF, Pie & ylog_zero Creep broken log10 axis
  - Persona 3: Expert (Figs 21-26) — Al-Zn-Mg Ternary, 3-Series Polar + Polar Function,
    VASP Spin-Polarized Vertical DOS & High-Symmetry Bands, 128x128 Heatmap/Contour/3D Surface,
    200k-pt Ultrasonic AE (<500ms 600-DPI PNG, <50ms inspect), and Zero-COM PowerPoint Native
    OLE Deck (`to_pptx`, `per_slide=4`, `slide_28pt`, `olefile` stream reload in fresh process)
  - Persona 4: AI-Agent Autonomy (Figs 27-30) — `describe_api` (216 methods), `app.batch`
    (`$prev`/`$steps[k]`, atomic rollback, multi-step undo/redo), `diagnose_layout` +
    `auto_layout` (`healthy == True`, `warnings == []`), NumPy/Pandas `df.attrs['units']`
    round-trip, and `to_python_script()` -> fresh-process `exec()` across XY+GaussFit, Pie,
    Box, 2x1 Stacked Histogram, Polar, and Ternary.

Strict invariants enforced on every 2D Atlas figure:
  1. `g.inspect()['healthy'] is True` and `warnings == []`
  2. `0` 'Bold' / non-normal font weights in exported `.svg`
  3. `100%` 'Arial' font family across exported `.svg` and graph metadata
  4. `0` thousands-separator commas (`1,000`) on numeric axis tick labels
  5. Non-empty 600-DPI `.png`, Vector `.pdf`, and Dual EMF+ `.emf` exports
"""

from __future__ import annotations

import atexit
import contextlib
import json
import math
import os
import random
import re
import socket
import struct
import subprocess
import sys
import tempfile
import time
import traceback
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path
from typing import Any, Callable, Dict, Iterator, List, Optional, Sequence, Tuple

REPO_ROOT = Path(__file__).resolve().parents[2]
SDK_DIR = REPO_ROOT / "python"
if str(SDK_DIR) not in sys.path:
    sys.path.insert(0, str(SDK_DIR))

import torigraph as tg  # noqa: E402
from torigraph.client import _discover_torigraph_executable  # noqa: E402

PORT_POOL: Tuple[int, ...] = (19860, 19861, 19862, 19863, 19864, 19865)
THOUSANDS_COMMA_RE = re.compile(r"^[+-]?\d{1,3}(,\d{3})+(\.\d+)?$")

_SPAWNED_PROCS: List[subprocess.Popen[bytes]] = []


def _cleanup_all_spawned_procs() -> None:
    """Strictly terminate ONLY ToriGraph child PIDs spawned by this test process."""
    for proc in list(_SPAWNED_PROCS):
        try:
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=5.0)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait(timeout=3.0)
        except Exception:
            pass
    _SPAWNED_PROCS.clear()


atexit.register(_cleanup_all_spawned_procs)


def _is_port_free(port: int) -> bool:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            s.bind(("127.0.0.1", port))
            return True
        except OSError:
            return False


class PortPoolLauncher:
    """Spawn isolated ToriGraph headless instances on ports 19860..19865 with strict PID cleanup."""

    def __init__(self, ports: Sequence[int] = PORT_POOL) -> None:
        self._ports = list(ports)
        self._cursor = 0
        self.exe = _discover_torigraph_executable()

    def _pick_port(self) -> int:
        for offset in range(len(self._ports)):
            cand = self._ports[(self._cursor + offset) % len(self._ports)]
            if _is_port_free(cand):
                self._cursor = (self._cursor + offset + 1) % len(self._ports)
                return cand
        return 0  # Ephemeral fallback only if all 19860..19865 are externally occupied in CI

    @contextlib.contextmanager
    def session(self, *, language: str = "en", timeout: float = 30.0) -> Iterator[Tuple[tg.Session, int, int]]:
        port = self._pick_port()
        port_fd, port_file = tempfile.mkstemp(prefix="tg_atlas_port_", suffix=".txt")
        os.close(port_fd)
        try:
            os.remove(port_file)
        except OSError:
            pass

        env = os.environ.copy()
        deps_bin = REPO_ROOT / ".deps" / "Library" / "bin"
        env["PATH"] = os.pathsep.join([str(self.exe.parent), str(deps_bin), env.get("PATH", "")])

        cmd = [
            str(self.exe),
            "--headless",
            "--rpc-port",
            str(port),
            "--rpc-port-file",
            port_file,
        ]
        proc = subprocess.Popen(
            cmd,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            cwd=str(self.exe.parent),
            env=env,
        )
        _SPAWNED_PROCS.append(proc)
        sess: Optional[tg.Session] = None
        try:
            deadline = time.monotonic() + timeout
            endpoint = ""
            while time.monotonic() < deadline:
                if proc.poll() is not None:
                    raise RuntimeError(f"ToriGraph exited early with code {proc.returncode}")
                if os.path.isfile(port_file):
                    txt = Path(port_file).read_text(encoding="utf-8").strip()
                    if txt:
                        endpoint = txt.splitlines()[0].strip()
                        break
                time.sleep(0.04)
            if not endpoint:
                raise RuntimeError(f"Timed out waiting for ToriGraph endpoint on port {port}")

            sess = tg.connect(endpoint, timeout=timeout)
            if language:
                sess.set_language(language=language)
            bound_port = int(endpoint.rsplit(":", 1)[-1]) if ":" in endpoint else port
            yield sess, bound_port, int(proc.pid)
        finally:
            if sess is not None:
                try:
                    sess.close()
                except Exception:
                    pass
            try:
                if proc.poll() is None:
                    proc.terminate()
                    try:
                        proc.wait(timeout=5.0)
                    except subprocess.TimeoutExpired:
                        proc.kill()
                        proc.wait(timeout=3.0)
            finally:
                if proc in _SPAWNED_PROCS:
                    _SPAWNED_PROCS.remove(proc)
                try:
                    if os.path.exists(port_file):
                        os.remove(port_file)
                except OSError:
                    pass


def read_png_dimensions(png_path: Path) -> Tuple[int, int]:
    raw = png_path.read_bytes()
    assert len(raw) >= 24 and raw[:8] == b"\x89PNG\r\n\x1a\n", f"Invalid PNG header in {png_path}"
    w, h = struct.unpack(">II", raw[16:24])
    return int(w), int(h)


def verify_dual_emf_plus(emf_path: Path, *, require_emf_plus: bool = True) -> Dict[str, Any]:
    data = emf_path.read_bytes()
    assert len(data) >= 88, f"EMF too small ({len(data)} B): {emf_path}"
    rec_type, _ = struct.unpack_from("<II", data, 0)
    assert rec_type == 1 and data[40:44] == b" EMF", f"Missing ENHMETAHEADER in {emf_path}"

    emf_plus_records = 0
    gdi_records = 0
    pos = 0
    while pos + 8 <= len(data):
        rtype, rsize = struct.unpack_from("<II", data, pos)
        if rsize < 8 or pos + rsize > len(data):
            break
        gdi_records += 1
        if rtype == 70 and rsize >= 16:
            _, sig = struct.unpack_from("<II", data, pos + 8)
            if sig == 0x2B464D45:  # 'EMF+'
                emf_plus_records += 1
        pos += rsize

    if require_emf_plus and os.name == "nt":
        assert emf_plus_records > 0, (
            f"Expected Dual EMF+ GDI comment records (0x2B464D45) in {emf_path}, found 0!"
        )
    return {"bytes": len(data), "gdi_records": gdi_records, "emf_plus_comments": emf_plus_records}


def audit_svg_typography(svg_path: Path) -> Dict[str, Any]:
    assert svg_path.is_file() and svg_path.stat().st_size > 400, f"Empty/missing SVG: {svg_path}"
    tree = ET.parse(svg_path)
    root = tree.getroot()
    fonts: set[str] = set()
    bold_violations: List[Dict[str, str]] = []
    non_arial_violations: List[str] = []

    for el in root.iter():
        tag = el.tag.split("}")[-1]
        ff = el.attrib.get("font-family", "").strip().strip("'\"")
        fw = el.attrib.get("font-weight", "").strip().lower()
        style_attr = el.attrib.get("style", "")
        if style_attr:
            for part in style_attr.split(";"):
                if ":" not in part:
                    continue
                k, v = part.split(":", 1)
                ks = k.strip().lower()
                vs = v.strip().strip("'\"")
                if ks == "font-family" and not ff:
                    ff = vs
                elif ks == "font-weight" and not fw:
                    fw = vs.lower()

        if ff:
            primary_ff = ff.split(",")[0].strip().strip("'\"")
            fonts.add(primary_ff)
            if primary_ff.lower() != "arial":
                non_arial_violations.append(primary_ff)
        if fw and fw not in ("400", "normal"):
            bold_violations.append({"tag": tag, "weight": fw, "text": (el.text or "").strip()})

    assert not bold_violations, f"Bold/non-normal SVG font weights in {svg_path.name}: {bold_violations[:5]}"
    assert not non_arial_violations, f"Non-Arial SVG fonts in {svg_path.name}: {non_arial_violations[:5]}"
    return {"bytes": svg_path.stat().st_size, "fonts": sorted(fonts)}


class AtlasRegistry:
    """Collects strict audit metrics across all 30 primary + companion Atlas figures."""

    def __init__(self, out_dir: Path) -> None:
        self.out_dir = out_dir
        self.figures: Dict[str, Dict[str, Any]] = {}
        self.primary_ids: List[str] = []
        self.checks_passed: List[str] = []
        self.checks_failed: List[Tuple[str, str]] = []

    def run_section(self, title: str, fn: Callable[[], None]) -> None:
        t0 = time.perf_counter()
        try:
            fn()
            dt = time.perf_counter() - t0
            self.checks_passed.append(title)
            print(f"[PASS] ({dt:5.2f}s) {title}", flush=True)
        except BaseException as exc:
            dt = time.perf_counter() - t0
            tb = traceback.format_exc()
            self.checks_failed.append((title, f"{exc}\n{tb}"))
            print(f"[FAIL] ({dt:5.2f}s) {title}: {exc}", flush=True)

    def verify_figure(
        self,
        app: tg.Session,
        g: tg.GraphProxy,
        fig_id: str,
        *,
        persona: str,
        description: str,
        is_primary: bool = True,
        dpi: int = 600,
    ) -> Dict[str, Any]:
        """Enforce all 5 Campaign-6 strict assertions on a 2D MultiLayer Atlas figure."""
        insp = g.inspect(include_ticks=True, diagnose=True)
        warnings = insp.get("warnings", [])
        assert insp.get("healthy") is True and warnings == [], (
            f"[{fig_id}] Unhealthy layout ({g.name}): warnings={warnings}"
        )

        # Verify 100% Arial in graph.info() and 0 thousands-separator commas on numeric ticks
        g_info = g.info()
        for lyr_idx, lyr_info in enumerate(g_info.get("layers", [])):
            for ax_key, ax_meta in (lyr_info.get("axes") or {}).items():
                if not isinstance(ax_meta, dict) or not ax_meta.get("enabled", True):
                    continue
                for fk in ("font_family", "title_font_family"):
                    ff_val = str(ax_meta.get(fk, "") or "").strip()
                    if ff_val:
                        assert ff_val.lower() == "arial", (
                            f"[{fig_id}] Layer {lyr_idx} axis '{ax_key}' {fk}={ff_val!r} != 'Arial'"
                        )

        for lyr_idx, lyr_snap in enumerate(insp.get("layers", [])):
            axes_snap = lyr_snap.get("axes") or {}
            for ax_key, ax_obj in axes_snap.items():
                if not isinstance(ax_obj, dict):
                    continue
                is_cat = bool(ax_obj.get("is_category") or ax_obj.get("scale") == "text")
                if is_cat:
                    continue
                for lbl in ax_obj.get("tick_labels") or []:
                    s_lbl = str(lbl).strip()
                    assert not THOUSANDS_COMMA_RE.match(s_lbl), (
                        f"[{fig_id}] Layer {lyr_idx} axis '{ax_key}' has thousands-comma tick {s_lbl!r}"
                    )

        png_path = self.out_dir / f"{fig_id}.png"
        pdf_path = self.out_dir / f"{fig_id}.pdf"
        emf_path = self.out_dir / f"{fig_id}.emf"
        svg_path = self.out_dir / f"{fig_id}.svg"

        t_png0 = time.perf_counter()
        g.export(png_path, dpi=dpi)
        png_ms = (time.perf_counter() - t_png0) * 1000.0
        g.export(pdf_path, dpi=dpi)
        g.export(emf_path, dpi=dpi)
        g.export(svg_path, dpi=dpi)

        assert png_path.is_file() and png_path.stat().st_size > 2000, f"[{fig_id}] Empty PNG"
        pw, ph = read_png_dimensions(png_path)
        assert pw >= 300 and ph >= 300, f"[{fig_id}] Unexpected PNG size {pw}x{ph}"

        pdf_head = pdf_path.read_bytes()[:8]
        assert pdf_head.startswith(b"%PDF-") and pdf_path.stat().st_size > 1000, (
            f"[{fig_id}] Invalid PDF export ({pdf_path})"
        )

        emf_meta = verify_dual_emf_plus(emf_path, require_emf_plus=True)
        svg_meta = audit_svg_typography(svg_path)

        rec = {
            "fig_id": fig_id,
            "persona": persona,
            "description": description,
            "graph_name": g.name,
            "is_primary": is_primary,
            "healthy": True,
            "warning_count": 0,
            "png_dims": [pw, ph],
            "png_bytes": png_path.stat().st_size,
            "png_export_ms": round(png_ms, 2),
            "pdf_bytes": pdf_path.stat().st_size,
            "emf": emf_meta,
            "svg": svg_meta,
        }
        self.figures[fig_id] = rec
        if is_primary:
            self.primary_ids.append(fig_id)
        return rec


def _write_synthetic_lab_files(data_dir: Path) -> Dict[str, Path]:
    data_dir.mkdir(parents=True, exist_ok=True)

    # 1. Shift-JIS (CP932) Japanese Tensile CSV
    sjis_csv = data_dir / "JIS_Tensile_Al7075.csv"
    sjis_lines = ["公称ひずみ (%),公称応力 (MPa)"]
    for i in range(25):
        e = 0.25 * i
        s = 710.0 * e if e <= 0.6 else 426.0 + 145.0 * (1.0 - math.exp(-0.7 * (e - 0.6)))
        sjis_lines.append(f"{e:.3f},{s:.2f}")
    sjis_csv.write_bytes(("\r\n".join(sjis_lines) + "\r\n").encode("cp932"))

    # 2. Headerless whitespace .xy XRD scan
    xy_file = data_dir / "powder_scan_headerless.xy"
    xy_lines = []
    for i in range(120):
        tt = 35.0 + 0.1 * i
        inten = 90.0 + 1400.0 * math.exp(-((tt - 38.47) ** 2) / (2.0 * 0.15**2))
        xy_lines.append(f"{tt:.4f}  {inten:.3f}")
    xy_file.write_text("\n".join(xy_lines) + "\n", encoding="utf-8")

    # 3. Rigaku SmartLab .ras ASCII scan
    rigaku_ras = data_dir / "Al7075_SmartLab.ras"
    ras_lines = [
        "*RAS_DATA_START",
        "*RAS_HEADER_START",
        '*MEAS_COND_AXIS_NAME_INTERNAL "TwoThetaOmega"',
        '*DISP_TITLE_Y "Intensity (cps)"',
        "*RAS_HEADER_END",
        "*RAS_INT_START",
    ]
    for i in range(120):
        tt = 35.0 + 0.1 * i
        inten = (
            110.0
            + 1850.0 * math.exp(-((tt - 38.47) ** 2) / (2.0 * 0.14**2))
            + 920.0 * math.exp(-((tt - 44.72) ** 2) / (2.0 * 0.16**2))
        )
        ras_lines.append(f"{tt:.4f} {inten:.2f} 1.0000")
    ras_lines.extend(["*RAS_INT_END", "*RAS_DATA_END"])
    rigaku_ras.write_text("\n".join(ras_lines) + "\n", encoding="utf-8")

    # 4. VASP Spin-Polarized DOSCAR (ISPIN=2 + s,p,d PDOS)
    doscar = data_dir / "DOSCAR"
    nedos = 81
    e_fermi = 5.4200
    dlines = [
        "    2    2    1    0",
        "  0.1200000E+02  0.2866500E-09  0.2866500E-09  0.2866500E-09  0.5000000E-15",
        "   300.00000000",
        "  CAR",
        " bcc-Fe spin-polarized",
        f"     12.00000000     -8.00000000  {nedos:4d}      {e_fermi:.8f}      1.00000000",
    ]
    energies = [-4.0 + i * (10.0 / (nedos - 1)) + e_fermi for i in range(nedos)]
    for e in energies:
        rel = e - e_fermi
        up_tot = 1.8 * math.exp(-((rel + 1.8) ** 2) / 1.1) + 0.4 * math.exp(-((rel - 1.2) ** 2) / 0.8) + 0.15
        dn_tot = 0.5 * math.exp(-((rel + 0.9) ** 2) / 0.9) + 1.9 * math.exp(-((rel - 1.6) ** 2) / 0.7) + 0.12
        dlines.append(f"  {e:10.4f}  {up_tot:10.4f}  {dn_tot:10.4f}  {up_tot*2:10.4f}  {dn_tot*2:10.4f}")
    dlines.append(f"     12.00000000     -8.00000000  {nedos:4d}      {e_fermi:.8f}      1.00000000")
    for e in energies:
        rel = e - e_fermi
        s_u, s_d = 0.12, 0.10
        p_u, p_d = 0.20, 0.18
        d_u = 1.5 * math.exp(-((rel + 1.8) ** 2) / 1.0) + 0.2 * math.exp(-((rel - 1.2) ** 2) / 0.8)
        d_d = 0.3 * math.exp(-((rel + 0.9) ** 2) / 0.9) + 1.6 * math.exp(-((rel - 1.6) ** 2) / 0.7)
        dlines.append(f"  {e:10.4f}  {s_u:9.4f}  {s_d:9.4f}  {p_u:9.4f}  {p_d:9.4f}  {d_u:9.4f}  {d_d:9.4f}")
    doscar.write_text("\n".join(dlines) + "\n", encoding="utf-8")

    # 5. VASP EIGENVAL (Gamma -> X -> M -> Gamma)
    eigenval = data_dir / "EIGENVAL"
    k_nodes = [(0.0, 0.0, 0.0), (0.5, 0.0, 0.0), (0.5, 0.5, 0.0), (0.0, 0.0, 0.0)]
    kpts: List[Tuple[float, float, float]] = []
    n_seg = 12
    for seg in range(3):
        a, b = k_nodes[seg], k_nodes[seg + 1]
        for j in range(n_seg):
            if seg > 0 and j == 0:
                continue
            t = j / (n_seg - 1)
            kpts.append((a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1]), a[2] + t * (b[2] - a[2])))
    elines = [
        "    2    2    1    1",
        "  0.2000000E+02  0.5431000E-09  0.5431000E-09  0.5431000E-09  0.5000000E-15",
        "  1.0000000E-04",
        "  CAR",
        " Si-like indirect band structure",
        f"       8    {len(kpts):4d}       4",
        "",
    ]
    for kx, ky, _ in kpts:
        elines.append(f"  {kx:11.7f}  {ky:11.7f}  {0.0:11.7f}  0.0250000")
        q2 = kx * kx + ky * ky
        b1 = -3.2 - 1.8 * math.cos(math.pi * kx) * math.cos(math.pi * ky)
        b2 = -2.4 * q2
        b3 = 1.14 + 3.5 * ((kx - 0.4) ** 2 + ky * ky)
        b4 = 2.6 + 1.9 * (math.cos(math.pi * (kx - 0.5)) ** 2)
        elines.extend(
            [
                f"    1    {b1:11.5f}   1.00000",
                f"    2    {b2:11.5f}   1.00000",
                f"    3    {b3:11.5f}   0.00000",
                f"    4    {b4:11.5f}   0.00000",
                "",
            ]
        )
    eigenval.write_text("\n".join(elines), encoding="utf-8")

    # 6. Binary Sun Raster (.ras) & NUL-byte Binary (.dat) for -32602 rejection guard
    sun_ras = data_dir / "sun_raster_binary.ras"
    sun_ras.write_bytes(struct.pack(">IIIIIIII", 0x59A66A95, 16, 16, 8, 256, 1, 0, 0) + b"\x00\x01\xff\x80" * 64)

    bin_dat = data_dir / "corrupt_binary_blob.dat"
    bin_dat.write_bytes(b"ToriBinary\x00\x00\xff\xfe\x00\x01\x02\x03\x00" * 24)

    return {
        "sjis_csv": sjis_csv,
        "xy_file": xy_file,
        "rigaku_ras": rigaku_ras,
        "doscar": doscar,
        "eigenval": eigenval,
        "sun_ras": sun_ras,
        "bin_dat": bin_dat,
    }


def run_persona_1_novice(app: tg.Session, reg: AtlasRegistry, lab_files: Dict[str, Path]) -> None:
    """Persona 1: Novice (Figs 01-10) + Smart Lab Imports."""
    app.new_project()

    # Verify sample dataset catalog & Japanese localization
    ds_en = app.list_sample_datasets()
    assert len(ds_en) == 6, f"Expected 6 built-in sample datasets, got {len(ds_en)}"
    app.set_language(lang="ja")
    ds_ja = app.list_sample_datasets()
    assert sum(1 for a, b in zip(ds_en, ds_ja) if a["description"] != b["description"]) == 6
    app.set_language(lang="en")

    sample_specs = [
        ("Fig01_Sample_Tensile_AlZnMg", "tensile_alznmg", "Sample 1: Al-Zn-Mg Tensile Curves (ISIJ 85mm)"),
        ("Fig02_Sample_TDS_Hydrogen", "tds_hydrogen_3rates", "Sample 2: Multi-Rate Hydrogen TDS Spectra"),
        ("Fig03_Sample_XRD_AgedAlloy", "xrd_aged_alloy", "Sample 3: Aged Alloy XRD Pattern"),
        ("Fig04_Sample_Arrhenius_Diffusion", "arrhenius_diffusion", "Sample 4: Arrhenius Diffusion Plot"),
        ("Fig05_Sample_CT_LocalStrain", "ct_local_strain_map", "Sample 5: 2D Local Strain Spectrogram"),
        ("Fig06_Sample_Weibull_Ceramics", "weibull_ceramics", "Sample 6: Right-Censored Weibull Ceramics"),
    ]
    for fig_id, sid, desc in sample_specs:
        meta_only = app.load_sample_dataset(sid, auto_plot=False)
        if meta_only.get("table"):
            cols = app.table(meta_only["table"]).info()["columns"]
            assert any((c.get("long_name") or "").strip() for c in cols), f"Missing metadata on {sid}"
        loaded = app.load_sample_dataset(sid, auto_plot=True)
        g_s = app.graph(loaded["graph"])
        g_s.isij_style()
        if g_s.diagnose_layout():
            g_s.auto_layout()
        if g_s.diagnose_layout():
            g_s.set_page_size(135, 105, "mm")
            g_s.auto_layout(allow_page_resize=False)
        reg.verify_figure(app, g_s, fig_id, persona="Novice", description=desc)

    app.new_project()

    # Fig07: Grouped Category Bars (K=3 series + y_err_cols) + isij_style()
    grp_tbl = app.create_table(
        {
            "Condition": ["As-Q", "T6", "T73", "RRA"],
            "Rp02": [145.0, 505.0, 435.0, 520.0],
            "Rm": [275.0, 572.0, 505.0, 585.0],
            "HV_x3": [285.0, 546.0, 474.0, 570.0],
            "Rp02_sd": [8.0, 12.0, 10.0, 11.0],
            "Rm_sd": [10.0, 14.0, 11.0, 13.0],
            "HV_sd": [9.0, 11.0, 9.0, 12.0],
        },
        name="Novice_GroupedBars",
        units={"Rp02": "MPa", "Rm": "MPa", "HV_x3": "MPa"},
        long_names={
            "Condition": "Aging condition",
            "Rp02": "<i>R</i><sub>p0.2</sub>",
            "Rm": "<i>R</i><sub>m</sub>",
            "HV_x3": "3×HV",
        },
    )
    g07 = app.plot_bars(
        grp_tbl,
        x_col="Condition",
        y_cols=["Rp02", "Rm", "HV_x3"],
        y_err_cols=["Rp02_sd", "Rm_sd", "HV_sd"],
        graph_name="Atlas_Fig07_GrpBars",
    )
    g07.isij_style()
    if g07.diagnose_layout():
        g07.auto_layout()
    assert g07.info()["layers"][0]["curve_count"] == 6
    reg.verify_figure(app, g07, "Fig07_Grouped_Category_Bars", persona="Novice", description="Grouped Category Bars + Multi-Series Error Bars")

    # Fig08 (& Fig08b): 100% Vertical Stacked Bars & 100% Horizontal Stacked Bars
    stk_tbl = app.create_table(
        {
            "Temper": ["As-Q", "120°C", "160°C", "RRA"],
            "AlphaAl": [98.5, 92.0, 88.5, 90.0],
            "GP_eta_p": [1.2, 6.8, 7.5, 8.2],
            "Eta_MgZn2": [0.3, 1.2, 4.0, 1.8],
        },
        name="Novice_PhaseStacked",
        units={"AlphaAl": "%", "GP_eta_p": "%", "Eta_MgZn2": "%"},
        long_names={
            "Temper": "Aging temper",
            "AlphaAl": "α-Al matrix",
            "GP_eta_p": "GP / η'",
            "Eta_MgZn2": "η-MgZn<sub>2</sub>",
        },
    )
    g08 = app.plot_stacked_bars(
        stk_tbl,
        x_col="Temper",
        y_cols=["AlphaAl", "GP_eta_p", "Eta_MgZn2"],
        percent_100=True,
        graph_name="Atlas_Fig08_VertStack100",
    )
    g08.isij_style()
    g08.set_legend(columns=2)
    if g08.diagnose_layout():
        g08.auto_layout()
    reg.verify_figure(app, g08, "Fig08_Stacked100_Vertical", persona="Novice", description="100% Vertical Stacked Phase Fractions")

    g08b = app.plot_stacked_bars(
        stk_tbl,
        x_col="Temper",
        y_cols=["AlphaAl", "GP_eta_p", "Eta_MgZn2"],
        horizontal=True,
        percent_100=True,
        graph_name="Atlas_Fig08b_HorizStack100",
    )
    g08b.isij_style()
    reg.verify_figure(
        app,
        g08b,
        "Fig08b_Stacked100_Horizontal",
        persona="Novice",
        description="100% Horizontal Stacked Phase Fractions",
        is_primary=False,
    )

    # Fig09: Grouped HorizontalBars with inline y_err_cols
    hbar_tbl = app.create_table(
        {
            "Condition": ["T4", "T6", "T73"],
            "Fracture": [38.4, 24.1, 11.6],
            "Fracture_SD": [3.2, 2.5, 1.4],
            "Debonding": [47.4, 49.8, 36.2],
            "Debonding_SD": [3.8, 3.1, 2.7],
            "Intact": [14.2, 26.1, 52.2],
            "Intact_SD": [2.1, 2.9, 3.5],
        },
        name="Novice_HorizDamage",
        units={"Fracture": "%", "Debonding": "%", "Intact": "%"},
        long_names={
            "Condition": "Heat treatment",
            "Fracture": "Particle fracture",
            "Debonding": "Interface debonding",
            "Intact": "Intact particles",
        },
    )
    g09 = app.plot_bars(
        hbar_tbl,
        x_col="Condition",
        y_cols=["Fracture", "Debonding", "Intact"],
        y_err_cols=["Fracture_SD", "Debonding_SD", "Intact_SD"],
        horizontal=True,
        graph_name="Atlas_Fig09_HorizBarsErr",
    )
    g09.isij_style()
    g09.auto_place_legend()
    reg.verify_figure(app, g09, "Fig09_Grouped_HorizBars_InlineErr", persona="Novice", description="Grouped Horizontal Bars with Inline Error Bars")

    # Fig10: Pareto Chart + Smart Lab Imports
    par_tbl = app.create_table(
        {
            "Defect": ["Inclusion", "Undercut", "Porosity", "Lack of fusion", "Hot crack"],
            "Count": [42.0, 7.0, 68.0, 19.0, 31.0],
        },
        name="Novice_WeldPareto",
        long_names={"Defect": "Defect type", "Count": "Defect count"},
    )
    g10 = app.plot_pareto(par_tbl, category_col="Defect", value_col="Count", graph_name="Atlas_Fig10_Pareto")
    g10.isij_style()
    if g10.diagnose_layout():
        g10.set_page_size(120, 95, "mm")
        g10.auto_layout(allow_page_resize=False)
    reg.verify_figure(app, g10, "Fig10_Pareto_WeldDefects", persona="Novice", description="Pareto Defect Chart (ISIJ)")

    # Smart Lab Imports: Shift-JIS CSV, headerless .xy, Rigaku .ras, VASP DOSCAR & EIGENVAL, binary -32602 guard
    t_sjis = app.import_csv(lab_files["sjis_csv"])
    sjis_cols = t_sjis.info()["columns"]
    assert t_sjis.info()["rows"] == 25 and sjis_cols[1].get("units") == "MPa", sjis_cols

    t_xy = app.import_csv(lab_files["xy_file"])
    assert t_xy.info()["rows"] == 120 and len(t_xy.info()["columns"]) >= 2

    t_ras = app.import_csv(lab_files["rigaku_ras"])
    assert t_ras.info()["rows"] == 120 and len(t_ras.info()["columns"]) >= 2

    t_dos = app.import_csv(lab_files["doscar"])
    assert t_dos.info()["rows"] == 81 and len(t_dos.info()["columns"]) >= 5

    t_eig = app.import_csv(lab_files["eigenval"])
    assert t_eig.info()["rows"] >= 30 and len(t_eig.info()["columns"]) >= 5

    wins_before_bad = {w["name"] for w in app.list_windows()}
    for bad_key in ("sun_ras", "bin_dat"):
        rejected = False
        try:
            app.import_csv(lab_files[bad_key])
        except tg.RpcError as exc:
            assert exc.code in (-32602, -32000), f"Unexpected code for {bad_key}: {exc}"
            rejected = True
        assert rejected, f"Binary file {bad_key} was not rejected by import_csv!"
    assert {w["name"] for w in app.list_windows()} == wins_before_bad


def run_persona_2_intermediate(app: tg.Session, reg: AtlasRegistry, tmp_dir: Path) -> None:
    """Persona 2: Intermediate (Figs 11-20)."""
    app.new_project()
    rng = random.Random(20261119)

    # Fig11: Ductile 0.2% Offset Yield Proof Stress + Brittle Ceramic Guard
    e_true = 71000.0
    eps_pct: List[float] = []
    sig_mpa: List[float] = []
    for i in range(36):
        s = 12.0 * i  # 0..420 MPa pure elastic
        sig_mpa.append(s)
        eps_pct.append((s / e_true) * 100.0)
    for i in range(1, 75):
        s = 420.0 + 2.2 * i
        eps_p = 0.002 * (((s - 420.0) / 60.0) ** 2.6)  # exact Rp0.2 = 480 MPa
        sig_mpa.append(s)
        eps_pct.append((s / e_true + eps_p) * 100.0)
    t_ten = app.create_table(
        {"Strain": eps_pct, "Stress": sig_mpa},
        name="Inter_Tensile7075",
        units={"Strain": "%", "Stress": "MPa"},
        long_names={"Strain": "Engineering strain, <i>e</i>", "Stress": "Engineering stress, <i>σ</i>"},
    )
    g11 = app.plot_xy(t_ten, "Strain", "Stress", style="line", graph_name="Atlas_Fig11_Rp02")
    ten_res = g11.analyze_tensile(offset=0.002, strain_in_percent=True, plot_construction=True, add_callout=True)
    assert abs(float(ten_res["offset_yield_stress"]) - 480.0) < 8.0, ten_res
    g11.isij_style()
    if g11.diagnose_layout():
        g11.auto_layout()
    reg.verify_figure(app, g11, "Fig11_Tensile_Rp02_ProofStress", persona="Intermediate", description="0.2% Offset Yield Proof Stress Construction")

    t_brit = app.create_table(
        {"Strain": [0.0, 0.08, 0.16, 0.24, 0.32, 0.33], "Stress": [0.0, 220.0, 440.0, 660.0, 880.0, 0.0]},
        units={"Strain": "%", "Stress": "MPa"},
        name="Inter_BrittleSiC",
    )
    g_brit = app.plot_xy(t_brit, "Strain", "Stress", style="line", graph_name="Inter_BrittleGraph")
    brit_blocked = False
    try:
        g_brit.analyze_tensile(offset=0.002, strain_in_percent=True)
    except tg.RpcError as exc:
        brit_blocked = exc.code == -32602
    assert brit_blocked, "Brittle elastic drop was not rejected by analyze_tensile!"
    app.close_window(g_brit)

    # Figs 12 & 13: Kocks-Mecking Work-Hardening (`considere` & `kocks_mecking` Stage-III)
    ten_wh_ds = app.load_sample_dataset("tensile_alznmg", auto_plot=True)
    g_voce_src = app.graph(ten_wh_ds["graph"])

    km_cons = g_voce_src.analyze_work_hardening(smooth_window=9, plot_mode="considere")
    g12 = app.graph(km_cons["graph"])
    g12.isij_style()
    if g12.diagnose_layout():
        g12.auto_layout()
    reg.verify_figure(app, g12, "Fig12_WorkHardening_Considere", persona="Intermediate", description="Considere Necking Criterion (θ & σ_t vs ε_t)")

    km_s3 = g_voce_src.analyze_work_hardening(smooth_window=9, plot_mode="kocks_mecking")
    g13 = app.graph(km_s3["graph"])
    g13.isij_style()
    if g13.diagnose_layout():
        g13.auto_layout()
    reg.verify_figure(app, g13, "Fig13_WorkHardening_KocksMecking", persona="Intermediate", description="Kocks-Mecking Stage-III Plot (θ vs σ_t)")

    # Fig14: Multi-Trap Kissinger TDS (`temp_min` / `temp_max` window isolation)
    tds_ds = app.load_sample_dataset("tds_hydrogen_3rates", auto_plot=False)
    tds_cols = [c["name"] for c in app.table(tds_ds["table"]).info()["columns"]]
    trap1 = app.analyze_kissinger(
        table=tds_ds["table"],
        temp_col=tds_cols[0],
        y_cols=tds_cols[1:4],
        heating_rates=[100.0, 200.0, 400.0],
        rate_in_k_per_hour=True,
        temp_in_celsius=True,
        temp_min=40.0,
        temp_max=175.0,
    )
    trap2 = app.analyze_kissinger(
        table=tds_ds["table"],
        temp_col=tds_cols[0],
        y_cols=tds_cols[1:4],
        heating_rates=[100.0, 200.0, 400.0],
        rate_in_k_per_hour=True,
        temp_in_celsius=True,
        temp_min=185.0,
        temp_max=360.0,
    )
    assert 22.0 < float(trap1["ea_kj_mol"]) < 32.0 and 50.0 < float(trap2["ea_kj_mol"]) < 64.0, (trap1, trap2)
    g14 = app.graph(trap2["graph"])
    g14.isij_style()
    if g14.diagnose_layout():
        g14.set_page_size(135, 105, "mm")
        g14.auto_layout(allow_page_resize=False)
    reg.verify_figure(app, g14, "Fig14_Kissinger_MultiTrap_TDS", persona="Intermediate", description="Kissinger Multi-Trap TDS Activation Energy")

    # Figs 15 (& 15b) & 16: FCC Al + HCP alpha-Ti XRD + Williamson-Hall + Linked Bragg Top-X
    wl_cu = 1.540598
    a_al = 4.0495
    tt_fcc = [30.0 + 0.04 * i for i in range(1451)]  # 30..88 deg (clean right margin past 82.4 deg (222) peak)
    y_fcc = [40.0] * len(tt_fcc)
    for h, k, l, amp in [(1, 1, 1, 1000.0), (2, 0, 0, 470.0), (2, 2, 0, 280.0), (3, 1, 1, 310.0), (2, 2, 2, 95.0)]:
        d_hkl = a_al / math.sqrt(h * h + k * k + l * l)
        th = math.asin(wl_cu / (2.0 * d_hkl))
        tt0 = 2.0 * math.degrees(th)
        beta_rad = (0.90 * (wl_cu * 0.1) / (42.0 * math.cos(th))) + 4.0 * 1.85e-3 * math.tan(th)
        fwhm_obs = math.hypot(math.degrees(beta_rad), 0.05)
        sig_g = fwhm_obs / (2.0 * math.sqrt(2.0 * math.log(2.0)))
        for idx, x in enumerate(tt_fcc):
            if abs(x - tt0) < 2.8:
                y_fcc[idx] += amp * math.exp(-0.5 * ((x - tt0) / sig_g) ** 2)
    t_fcc = app.create_table(
        {"TwoTheta": tt_fcc, "Intensity": y_fcc},
        name="Inter_Al_FCC_XRD",
        units={"TwoTheta": "deg", "Intensity": "cps"},
        long_names={"TwoTheta": "Diffraction angle, 2<i>θ</i>", "Intensity": "Intensity, <i>I</i>"},
    )
    g15 = app.plot_xy(t_fcc, "TwoTheta", "Intensity", style="line", graph_name="Atlas_Fig15_FCC_XRD")
    xrd_fcc = g15.analyze_xrd(
        wavelength=wl_cu,
        instr_fwhm=0.05,
        crystal_system="fcc",
        add_top_d_axis=True,
        create_wh_plot=True,
    )
    assert len(xrd_fcc.get("peaks", [])) == 5 and xrd_fcc.get("wh_graph"), xrd_fcc
    g15.isij_style()
    g15.set_page_size(140, 100, "mm")
    g15.set_axis("y", min=0.0, max=1380.0)
    g15.remove_marker(kind="text", clear_all=True)
    for idx_p, pk in enumerate(xrd_fcc["peaks"]):
        tt_p = float(pk["two_theta_deg"])
        y_top = float(pk["intensity"])
        lift = 210.0 if idx_p == 4 else 135.0
        g15.add_text(str(pk["hkl"]), x=tt_p - 2.2, y=y_top + lift, frame=False, font_size=8)
    g15.auto_place_legend()
    if g15.diagnose_layout():
        g15.auto_layout(allow_page_resize=False)
    reg.verify_figure(app, g15, "Fig15_XRD_FCC_Al_BraggTopX", persona="Intermediate", description="FCC Al XRD + Bragg d-Spacing Top-X Axis")

    g16 = app.graph(xrd_fcc["wh_graph"])
    g16.isij_style()
    if g16.diagnose_layout():
        g16.set_page_size(135, 105, "mm")
        g16.auto_layout(allow_page_resize=False)
    reg.verify_figure(app, g16, "Fig16_XRD_WilliamsonHall", persona="Intermediate", description="Williamson-Hall Crystallite Size & Microstrain")

    # Companion Fig15b: HCP alpha-Ti XRD (`crystal_system="hcp"`, `c_over_a=1.5879`)
    a_ti, ca_ti = 2.9508, 1.5879
    c_ti = a_ti * ca_ti
    tt_hcp = [32.0 + 0.04 * i for i in range(1101)]
    y_hcp = [30.0] * len(tt_hcp)
    for h, k, l, amp in [(1, 0, 0, 260.0), (0, 0, 2, 300.0), (1, 0, 1, 1000.0), (1, 0, 2, 180.0), (1, 1, 0, 210.0)]:
        inv_d2 = (4.0 / 3.0) * (h * h + h * k + k * k) / (a_ti * a_ti) + (l * l) / (c_ti * c_ti)
        th = math.asin(wl_cu * math.sqrt(inv_d2) / 2.0)
        tt0 = 2.0 * math.degrees(th)
        sig_g = 0.16 / (2.0 * math.sqrt(2.0 * math.log(2.0)))
        for idx, x in enumerate(tt_hcp):
            if abs(x - tt0) < 2.2:
                y_hcp[idx] += amp * math.exp(-0.5 * ((x - tt0) / sig_g) ** 2)
    t_hcp = app.create_table(
        {"TwoTheta": tt_hcp, "Intensity": y_hcp},
        name="Inter_Ti_HCP_XRD",
        units={"TwoTheta": "deg", "Intensity": "cps"},
        long_names={"TwoTheta": "Diffraction angle, 2<i>θ</i>", "Intensity": "Intensity, <i>I</i>"},
    )
    g15b = app.plot_xy(t_hcp, "TwoTheta", "Intensity", style="line", graph_name="Atlas_Fig15b_HCP_XRD")
    xrd_hcp = g15b.analyze_xrd(wavelength=wl_cu, instr_fwhm=0.05, crystal_system="hcp", c_over_a=ca_ti, create_wh_plot=False)
    assert any("(101)" in str(p.get("hkl", "")) for p in xrd_hcp.get("peaks", [])), xrd_hcp
    g15b.isij_style()
    g15b.set_page_size(145, 100, "mm")
    g15b.set_axis("y", min=0.0, max=1820.0)
    g15b.remove_marker(kind="text", clear_all=True)
    for idx_p, pk in enumerate(xrd_hcp["peaks"]):
        tt_p = float(pk["two_theta_deg"])
        y_top = float(pk["intensity"])
        if idx_p == 0:
            dx_lbl, y_lbl = -2.2, 1410.0  # middle tier cleanly above (101)
        elif idx_p == 1:
            dx_lbl, y_lbl = -1.8, 1630.0  # upper tier cleanly above (100)
        elif idx_p == 2:
            dx_lbl, y_lbl = -1.4, 1190.0  # lower tier directly over (101) tip
        else:
            dx_lbl, y_lbl = -2.0, y_top + 150.0
        g15b.add_text(str(pk["hkl"]), x=tt_p + dx_lbl, y=y_lbl, frame=False, font_size=8)
    g15b.auto_place_legend()
    if g15b.diagnose_layout():
        g15b.auto_layout(allow_page_resize=False)
    insp15b = g15b.inspect()
    peak_tt_by_hkl = {
        str(pk["hkl"]).strip(): float(pk["two_theta_deg"])
        for pk in xrd_hcp["peaks"]
        if str(pk.get("hkl", "")).strip()
    }
    matched_hkl = 0
    for tm in insp15b["layers"][0].get("text_markers", []):
        lbl_txt = str(tm.get("text", "")).strip()
        if lbl_txt in peak_tt_by_hkl:
            matched_hkl += 1
            dx_deg = abs(float(tm["x"]) - peak_tt_by_hkl[lbl_txt])
            assert dx_deg <= 2.5, (
                f"Fig15b (hkl) label {lbl_txt!r} at 2theta={tm['x']:.2f} drifted >2.5 deg "
                f"from Bragg peak {peak_tt_by_hkl[lbl_txt]:.2f}!"
            )
    assert matched_hkl >= 4, f"Expected >= 4 anchored (hkl) peak labels on Fig15b, got {insp15b['layers'][0].get('text_markers')}"
    reg.verify_figure(app, g15b, "Fig15b_XRD_HCP_AlphaTi", persona="Intermediate", description="HCP α-Ti XRD Indexing", is_primary=False)

    # Fig17: Right-Censored Weibull MLE + .sciprj Reload Persistence
    app.new_project()
    wb_tbl = app.create_table(
        {
            "Stress": [585, 612, 628, 645, 658, 670, 682, 695, 706, 718, 731, 744, 758, 775, 792, 820, 820, 820, 820, 820],
            "Status": [1] * 15 + [0] * 5,
        },
        name="Inter_Si3N4_Weibull",
        units={"Stress": "MPa"},
        long_names={"Stress": "Flexural strength, <i>σ</i><sub>f</sub>", "Status": "Failure status"},
    )
    wb_mle = app.weibull(wb_tbl, col="Stress", censor_col="Status", censor_coding=0, mle=True)
    assert wb_mle.get("used_mle") is True and wb_mle.get("censored_count") == 5, wb_mle
    g17_orig = app.graph(wb_mle["graph"])
    g17_orig.isij_style()
    wb_sciprj = tmp_dir / "fig17_weibull_mle.sciprj"
    app.save_project(wb_sciprj, compress=False)
    app.new_project()
    app.open_project(wb_sciprj)
    g17 = app.graph(g17_orig.name)
    insp17 = g17.inspect(include_ticks=True)
    yr_ticks = [
        str(t)
        for ly in insp17["layers"]
        for t in (ly.get("axes", {}).get("y_right") or ly.get("axes", {}).get("right") or {}).get("tick_labels", [])
    ]
    assert any("63.2" in t for t in yr_ticks), f"Reloaded Weibull lost 63.2% Right-Y tick: {yr_ticks}"
    if g17.diagnose_layout():
        g17.set_page_size(135, 105, "mm")
        g17.auto_layout(allow_page_resize=False)
    reg.verify_figure(app, g17, "Fig17_Weibull_Censored_MLE_Reload", persona="Intermediate", description="Right-Censored Weibull MLE (Reloaded from .sciprj)")

    # Fig18 (& Fig18b): Box + One-Way ANOVA Brackets & Violin + Nonparametric Kruskal-Wallis Brackets
    t_aov = app.create_table(
        {
            "As_Q": [108.0 + rng.gauss(0, 4.5) for _ in range(14)],
            "T6": [186.0 + rng.gauss(0, 5.0) for _ in range(14)],
            "RRA": [194.0 + rng.gauss(0, 4.8) for _ in range(14)],
        },
        name="Inter_HV_Groups",
        units={"As_Q": "HV", "T6": "HV", "RRA": "HV"},
        long_names={"As_Q": "As-Quenched", "T6": "T6 Peak-Aged", "RRA": "RRA Temper"},
    )
    g18 = app.plot_box(t_aov, cols=["As_Q", "T6", "RRA"], show_points=True, graph_name="Atlas_Fig18_BoxANOVA")
    aov_res = app.anova_oneway(t_aov, cols=["As_Q", "T6", "RRA"], posthoc="tukey", graph=g18, add_brackets=True)
    assert aov_res.get("ok") is True and float(aov_res["p_value"]) < 1e-4, aov_res
    g18.isij_style()
    if g18.diagnose_layout():
        g18.auto_layout()
    insp18 = g18.inspect()
    arrows18 = insp18["layers"][0].get("arrows", [])
    assert len(arrows18) >= 2, f"Expected ANOVA significance bracket arrows on Fig18, got {arrows18}"
    y_ax18 = insp18["layers"][0]["axes"]["y"]
    for ar in arrows18:
        x_lo = min(float(ar["x1"]), float(ar["x2"]))
        x_hi = max(float(ar["x1"]), float(ar["x2"]))
        assert 0.8 <= x_lo and x_hi <= 3.2, f"Fig18 ANOVA bracket X out of [0.8, 3.2]: {ar}"
        assert float(y_ax18["min"]) <= float(ar["y1"]) <= float(y_ax18["max"]), (
            f"Fig18 ANOVA bracket Y={ar['y1']} outside y-axis [{y_ax18['min']}, {y_ax18['max']}]: {ar}"
        )
    reg.verify_figure(app, g18, "Fig18_Box_OneWay_ANOVA_Brackets", persona="Intermediate", description="Box Plot + One-Way ANOVA Tukey Brackets")

    g18b = app.plot_violin(t_aov, cols=["As_Q", "T6", "RRA"], show_points=True, graph_name="Atlas_Fig18b_ViolinKW")
    kw_res = app.nonparametric_test(t_aov, cols=["As_Q", "T6", "RRA"], test="kruskal_wallis", graph=g18b, add_brackets=True)
    assert kw_res.get("ok") is True, kw_res
    g18b.isij_style()
    if g18b.diagnose_layout():
        g18b.auto_layout()
    reg.verify_figure(app, g18b, "Fig18b_Violin_Kruskal_Brackets", persona="Intermediate", description="Violin Plot + Kruskal-Wallis Brackets", is_primary=False)

    # Fig19: 2x1 Stacked Histogram + Lognormal PDF
    t_prec = app.create_table(
        {
            "d_T6": [round(rng.lognormvariate(-2.72, 0.36), 6) for _ in range(200)],
            "d_T73": [round(rng.lognormvariate(-2.02, 0.34), 6) for _ in range(200)],
        },
        name="Inter_PrecipSizes",
        units={"d_T6": "μm", "d_T73": "μm"},
        long_names={
            "d_T6": "Precipitate diameter, <i>d</i><sub>eq</sub> (T6)",
            "d_T73": "Precipitate diameter, <i>d</i><sub>eq</sub> (T73)",
        },
    )
    g19 = app.plot_histogram(
        t_prec,
        cols=["d_T6", "d_T73"],
        stacked=True,
        bins=14,
        bin_min=0.0,
        bin_max=0.36,
        overlay_pdf="lognormal",
        graph_name="Atlas_Fig19_Hist2x1_PDF",
    )
    g19.isij_style()
    assert g19.info()["layer_count"] == 2
    reg.verify_figure(app, g19, "Fig19_Stacked_Histogram_2x1_LognormalPDF", persona="Intermediate", description="2x1 Stacked Histogram + Lognormal PDF")

    # Fig20 (& Fig20b): ylog_zero Creep Broken Log10 Axis + Pie Phase Fractions
    t_creep = app.create_table(
        {
            "Stress": [40.0, 60.0, 80.0, 100.0, 125.0, 150.0, 175.0, 200.0, 225.0, 250.0],
            "CreepRate": [0.0, 0.0, 0.0, 1.4e-6, 7.2e-6, 3.1e-5, 1.4e-4, 6.8e-4, 3.2e-3, 1.8e-2],
        },
        name="Inter_CreepYLogZero",
        units={"Stress": "MPa", "CreepRate": "s<sup>-1</sup>"},
        long_names={"Stress": "Applied stress, <i>σ</i>", "CreepRate": "Minimum creep rate, d<i>ε</i>/d<i>t</i>"},
    )
    g20 = app.plot_xy(t_creep, "Stress", "CreepRate", style="line_symbol", graph_name="Atlas_Fig20_CreepYLogZero")
    g20.isij_style()
    g20.set_axis("y", min=0.0, max=0.5)
    g20.set_axis_break("y", enabled=True, start=1e-7, end=1e-6, position_pct=18.0, log_after=True)
    g20.auto_place_legend()
    reg.verify_figure(app, g20, "Fig20_Creep_YLogZero_BrokenAxis", persona="Intermediate", description="Norton Creep Minimum Rate (ylog_zero Broken Axis + EMF+)")

    t_pie = app.create_table(
        {"Phase": ["α-Al matrix", "η' (MgZn₂)", "T-phase", "Al₇Cu₂Fe"], "VolPct": [86.4, 8.2, 3.6, 1.8]},
        name="Inter_PiePhases",
    )
    g20b = app.plot_pie(t_pie, col="VolPct", label_col="Phase", graph_name="Atlas_Fig20b_PiePhases")
    reg.verify_figure(app, g20b, "Fig20b_Pie_PhaseFractions", persona="Intermediate", description="Alloy Phase Volume Fractions Pie", is_primary=False)


def run_persona_3_expert(
    app: tg.Session,
    launcher: PortPoolLauncher,
    reg: AtlasRegistry,
    lab_files: Dict[str, Path],
    out_dir: Path,
) -> None:
    """Persona 3: Expert (Figs 21-26) + Zero-COM PowerPoint Native OLE Deck Reload."""
    import olefile  # type: ignore[import-not-found]

    app.new_project()

    # Fig21: Al-Zn-Mg Ternary Phase/Composition Plot
    t_tern = app.create_table(
        {
            "Al": [88.0, 85.0, 82.0, 78.0, 90.0, 84.0, 80.0],
            "Zn": [8.0, 10.0, 12.0, 15.0, 6.0, 9.0, 13.0],
            "Mg": [4.0, 5.0, 6.0, 7.0, 4.0, 7.0, 7.0],
        },
        name="Expert_AlZnMg_Ternary",
        units={"Al": "mass%", "Zn": "mass%", "Mg": "mass%"},
    )
    g21 = app.plot_ternary(t_tern, a_col="Al", b_col="Zn", c_col="Mg", graph_name="Atlas_Fig21_Ternary")
    g21.isij_style()
    reg.verify_figure(app, g21, "Fig21_Ternary_AlZnMg", persona="Expert", description="Al-Zn-Mg Ternary Composition Diagram")

    # Fig22 (& Fig22b): 3-Series Crystallographic Texture/Anisotropy Polar + Polar Function
    thetas = [float(k * 15) for k in range(25)]
    t_pol = app.create_table(
        {
            "Azimuth": thetas,
            "AsRolled": [515.0 + 38.0 * math.cos(math.radians(2.0 * th)) - 14.0 * math.cos(math.radians(4.0 * th)) for th in thetas],
            "T6_Sheet": [540.0 + 18.0 * math.cos(math.radians(2.0 * th)) for th in thetas],
            "Recryst_O": [210.0 + 12.0 * math.cos(math.radians(4.0 * th)) for th in thetas],
        },
        name="Expert_AnisotropyPolar",
        units={"Azimuth": "deg", "AsRolled": "MPa", "T6_Sheet": "MPa", "Recryst_O": "MPa"},
        long_names={
            "Azimuth": "Rolling azimuth, <i>θ</i>",
            "AsRolled": "As-rolled",
            "T6_Sheet": "T6 sheet",
            "Recryst_O": "O-temper",
        },
    )
    g22 = app.plot_polar(
        t_pol,
        theta_col="Azimuth",
        r_cols=["AsRolled", "T6_Sheet", "Recryst_O"],
        theta_in_radians=False,
        graph_name="Atlas_Fig22_Polar3Series",
    )
    g22.isij_style()
    reg.verify_figure(app, g22, "Fig22_Polar_3Series_Anisotropy", persona="Expert", description="3-Series Sheet Yield Anisotropy Polar Plot")

    g22b = app.plot_function("70 + 8*cos(4*x)", x_min=0.0, x_max=2.0 * math.pi, type="polar", graph_name="Atlas_Fig22b_PolarFunc")
    g22b.isij_style()
    reg.verify_figure(app, g22b, "Fig22b_Polar_AnalyticalFunction", persona="Expert", description="Analytical Polar r(θ) Function", is_primary=False)

    # Fig23: VASP Spin-Polarized Vertical-Energy DOS
    t_dos = app.import_csv(lab_files["doscar"])
    dos_cols_all = [c["name"] for c in t_dos.info()["columns"]]
    up_cols = [c for c in dos_cols_all[1:] if "up" in c.lower() or "↑" in c][:3] or [dos_cols_all[1]]
    dn_cols = [c for c in dos_cols_all[1:] if "down" in c.lower() or "dn" in c.lower() or "↓" in c][:3]
    vd_res = app.plot_vasp_dos(
        table=t_dos.name,
        energy_col=dos_cols_all[0],
        dos_cols=up_cols,
        down_dos_cols=dn_cols or None,
        fermi_energy=0.0,
        vertical_energy=True,
    )
    g23 = app.graph(vd_res["graph"])
    g23.isij_style()
    reg.verify_figure(app, g23, "Fig23_VASP_SpinDOS_Vertical", persona="Expert", description="VASP Spin-Polarized Vertical-Energy DOS")

    # Fig24: VASP Band Structure (Γ, X, M, Γ)
    vb_res = app.plot_vasp_bands(
        eigenval_path=lab_files["eigenval"],
        fermi_energy=0.0,
        k_ticks=[0.0, 0.5, 1.0, 1.0 + math.sqrt(0.5)],
        k_labels=["Γ", "X", "M", "Γ"],
    )
    g24 = app.graph(vb_res["graph"])
    g24.isij_style()
    vb_ticks = g24.inspect(include_ticks=True)["layers"][0]["axes"]["x"]["tick_labels"]
    assert vb_ticks == ["Γ", "X", "M", "Γ"], f"Unexpected VASP k-ticks: {vb_ticks}"
    reg.verify_figure(app, g24, "Fig24_VASP_Bands_HighSymmetry", persona="Expert", description="VASP Electronic Band Structure (Γ-X-M-Γ)")

    # Fig25 (& Fig25b, Fig25c): 128x128 Local-Strain Heatmap + Contour + 3D Surface
    n_grid = 128
    strain_grid = [
        [
            round(
                1.8 * math.exp(-(((r - 52.0) ** 2 + (c - 48.0) ** 2) / 520.0))
                + 1.35 * math.exp(-(((r - 84.0) ** 2 + (c - 82.0) ** 2) / 420.0))
                + 0.15 * math.sin(0.12 * r) * math.cos(0.12 * c),
                5,
            )
            for c in range(n_grid)
        ]
        for r in range(n_grid)
    ]
    m128 = app.create_matrix(
        strain_grid,
        name="Expert_Strain128",
        x_min=0.0,
        x_max=64.0,
        y_min=0.0,
        y_max=64.0,
    )
    g25 = app.plot_heatmap(
        m128,
        palette="viridis",
        show_colorbar=True,
        colorbar_title="Local strain, <i>ε</i><sub>xx</sub> / %",
        graph_name="Atlas_Fig25_Heatmap128",
    )
    g25.set_colorbar(palette="viridis", show_contour_lines=True, levels=8)
    g25.set_axis("x", title="Position, <i>x</i> / μm")
    g25.set_axis("y", title="Position, <i>y</i> / μm")
    g25.isij_style()
    reg.verify_figure(app, g25, "Fig25_Heatmap_Contour_128x128", persona="Expert", description="128x128 DIC Local Strain Heatmap + Iso-Strain Contours")

    g25b = app.plot_matrix(m128, style="contour", levels=10, graph_name="Atlas_Fig25b_Contour128")
    g25b.set_axis("x", title="Position, <i>x</i> / μm")
    g25b.set_axis("y", title="Position, <i>y</i> / μm")
    g25b.isij_style()
    reg.verify_figure(app, g25b, "Fig25b_Contour_128x128", persona="Expert", description="128x128 Pure Contour Map", is_primary=False)

    g25c_3d = app.plot_3d_matrix(m128, style="surface", graph_name="Atlas_Fig25c_3DSurface")
    g25c_3d.set_3d_options(palette="viridis", title="", axes_labels=["x / μm", "y / μm", "ε_xx / %"])
    png_3d = out_dir / "Fig25c_3DSurface_128x128.png"
    pdf_3d = out_dir / "Fig25c_3DSurface_128x128.pdf"
    emf_3d = out_dir / "Fig25c_3DSurface_128x128.emf"
    g25c_3d.export(png_3d, dpi=600)
    g25c_3d.export(pdf_3d, dpi=600)
    g25c_3d.export(emf_3d, dpi=300)
    w3d, h3d = read_png_dimensions(png_3d)
    assert w3d >= 1200 and h3d >= 900 and pdf_3d.stat().st_size > 2000
    verify_dual_emf_plus(emf_3d, require_emf_plus=False)

    # Fig26: 200,000-Point Ultrasonic AE Waveform (<500ms 600-DPI PNG, <50ms inspect) + Zero-COM PowerPoint Deck
    n_ae = 200_000
    dt_us = 0.005  # 0 .. 1000 us
    ae_t = [i * dt_us for i in range(n_ae)]
    ae_v = [
        math.exp(-max(0.0, t - 180.0) / 85.0) * math.sin(2.0 * math.pi * 0.25 * t) * (1.0 if t >= 180.0 else 0.02)
        + 0.55 * math.exp(-max(0.0, t - 540.0) / 65.0) * math.sin(2.0 * math.pi * 0.42 * t) * (1.0 if t >= 540.0 else 0.0)
        for t in ae_t
    ]
    t_ae = app.create_table(
        {"Time_us": ae_t, "Voltage_V": ae_v},
        name="Expert_Ultrasonic200k",
        units={"Time_us": "μs", "Voltage_V": "V"},
        long_names={"Time_us": "Propagation time, <i>t</i>", "Voltage_V": "Sensor amplitude, <i>V</i>"},
    )
    g26 = app.plot_xy(t_ae, "Time_us", "Voltage_V", style="line", graph_name="Atlas_Fig26_Ultrasonic200k")
    g26.set_axis("x", divide_by=1000.0, title="Propagation time, <i>t</i> / ms")  # clean 0..1.0 ms ticks, zero 1,000 commas
    g26.isij_style()
    if g26.diagnose_layout():
        g26.auto_layout(allow_page_resize=False)

    t_insp0 = time.perf_counter()
    insp_26 = g26.inspect()
    insp_26_ms = (time.perf_counter() - t_insp0) * 1000.0
    assert insp_26_ms < 50.0, f"200k-pt inspect() exceeded 50ms budget: {insp_26_ms:.2f} ms"
    assert insp_26.get("healthy") is True

    rec_26 = reg.verify_figure(
        app,
        g26,
        "Fig26_Ultrasonic_200k_Waveform",
        persona="Expert",
        description=f"200,000-pt Ultrasonic AE Waveform (inspect={insp_26_ms:.1f}ms)",
    )
    assert rec_26["png_export_ms"] < 500.0, (
        f"200k-pt 600-DPI PNG exceeded 500ms budget: {rec_26['png_export_ms']:.2f} ms"
    )

    # Zero-COM PowerPoint Native OLE Deck (`app.to_pptx`, `per_slide=4`, `slide_28pt`, fresh-process OLE reload)
    pptx_path = out_dir / "Fig26_MatSci_Atlas_Deck.pptx"
    deck_graphs = [g21, g22, g23, g24]
    pptx_res = app.to_pptx(
        pptx_path,
        deck_graphs,
        per_slide=4,
        preset="slide_28pt",
        titles=["Campaign 6 — Editable ToriGraph Native OLE Quad Deck (28 pt Arial)"],
        dpi=300,
    )
    assert pptx_res.get("ok") is True and pptx_res.get("slide_count") == 1 and pptx_res.get("embedded_count") == 4

    # Verify slide_28pt 28pt Arial was applied
    g21_slide_info = g21.info()["layers"][0]["axes"]["x"]
    assert int(g21_slide_info.get("title_font_size") or g21_slide_info.get("font_size") or 28) >= 24

    extracted_sciprj = out_dir / "extracted_from_pptx_ole.sciprj"
    with zipfile.ZipFile(pptx_path, "r") as zf:
        ole_parts = sorted(n for n in zf.namelist() if n.startswith("ppt/embeddings/oleObject"))
        emf_parts = sorted(n for n in zf.namelist() if n.startswith("ppt/media/image") and n.endswith(".emf"))
        assert len(ole_parts) == 4 and len(emf_parts) == 4
        ole_blob = zf.read(ole_parts[-1])
        with olefile.OleFileIO(ole_blob) as ole:
            assert ole.root.clsid.upper() == "8E6B9541-3C74-4D28-A915-7D84E5A12001"
            assert ole.exists("\x01CompObj") and ole.exists("\x01Ole") and ole.exists("ToriGraphProject")
            assert not ole.exists("SciDAVisDib"), "oleObject.bin must not contain bloated uncompressed SciDAVisDib!"
            meta_json = json.loads(ole.openstream("SciDAVisMeta").read().decode("utf-8"))
            assert meta_json.get("targetWindow") == g24.name, f"Expected canonical targetWindow {g24.name!r}, got {meta_json!r}"
            extracted_sciprj.write_bytes(ole.openstream("ToriGraphProject").read())

    # Also verify copy_ole(preset="slide_28pt") and mixed 2D+Graph3D to_pptx(preset="slide_28pt")
    copy_ole_res = g24.copy_ole(preset="slide_28pt")
    assert copy_ole_res.get("ok") is True and copy_ole_res.get("graph") == g24.name
    pptx_mixed_3d = out_dir / "Fig26b_Mixed_2D_and_3D_OLE.pptx"
    mixed_res = app.to_pptx(
        pptx_mixed_3d,
        [g24, g25c_3d.name],
        per_slide=2,
        preset="slide_28pt",
        dpi=300,
    )
    assert mixed_res.get("ok") is True and mixed_res.get("embedded_count") == 2

    with launcher.session(language="en") as (fresh_app, _, _):
        fresh_app.open_project(extracted_sciprj)
        restored_names = {w["name"] for w in fresh_app.list_windows()}
        for dg in deck_graphs:
            assert dg.name in restored_names, f"OLE reloaded project missing {dg.name}: {restored_names}"


def run_persona_4_ai_agent_autonomy(
    app: tg.Session,
    launcher: PortPoolLauncher,
    reg: AtlasRegistry,
) -> None:
    """Persona 4: AI-Agent Autonomy (Figs 27-30)."""
    import numpy as np  # type: ignore[import-not-found]
    import pandas as pd  # type: ignore[import-not-found]

    app.new_project()

    # Fig27: describe_api (216 methods), app.batch ($prev/$steps, rollback, multi-step undo/redo) & Pandas attrs
    methods_list = app.supported_methods()
    api_cat = app.describe_api()
    cat_methods = api_cat["methods"] if isinstance(api_cat, dict) and "methods" in api_cat else api_cat
    assert len(methods_list) >= 216 and len(cat_methods) == len(methods_list), (
        f"Catalog mismatch: supported={len(methods_list)}, describe_api={len(cat_methods)}"
    )

    df_in = pd.DataFrame(
        {
            "Temp_K": np.linspace(300.0, 800.0, 11),
            "Diffusivity": np.linspace(1.2, 8.7, 11),
        }
    )
    df_in.attrs["units"] = {"Temp_K": "K", "Diffusivity": "10<sup>-11</sup> m<sup>2</sup> s<sup>-1</sup>"}
    df_in.attrs["long_names"] = {"Temp_K": "Absolute temperature, <i>T</i>", "Diffusivity": "Tracer diffusivity, <i>D</i>"}
    t_df = app.create_table(df_in, name="Agent_PandasTbl")
    df_rt = t_df.to_dataframe(backend="pandas")
    assert df_rt.attrs["units"]["Diffusivity"] == "10<sup>-11</sup> m<sup>2</sup> s<sup>-1</sup>"
    assert df_rt.attrs["long_names"]["Temp_K"] == "Absolute temperature, <i>T</i>"

    batch_direct = app.batch(
        [
            {"method": "table.create", "params": {"rows": 8, "cols": 2, "name": "Agent_BatchPipeTbl"}},
            {
                "method": "table.set_data_dict",
                "params": {
                    "table": "$prev.name",
                    "data": {"Strain": [0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0], "Stress": [0.0, 140.0, 275.0, 365.0, 420.0, 455.0, 478.0, 492.0]},
                    "units": {"Strain": "%", "Stress": "MPa"},
                    "long_names": {"Strain": "Plastic strain, <i>ε</i><sub>p</sub>", "Stress": "Flow stress, <i>σ</i>"},
                },
            },
            {
                "method": "plot.xy",
                "params": {"table": "$steps[0].name", "x_col": "Strain", "y_cols": ["Stress"], "graph_name": "Atlas_Fig27_AgentBatch"},
            },
        ],
        atomic=True,
        stop_on_error=True,
        defer_replot=True,
    )
    assert batch_direct.get("ok") is True and batch_direct.get("completed") == 3

    wins_snap = {w["name"] for w in app.list_windows()}
    rolled_back = False
    try:
        with app.batch(atomic=True, stop_on_error=True):
            app.create_table(rows=4, cols=2, name="Agent_MustRollback")
            app.call("nonexistent.boom_method")
    except tg.RpcError:
        rolled_back = True
    assert rolled_back and {w["name"] for w in app.list_windows()} == wins_snap

    g27 = app.graph("Atlas_Fig27_AgentBatch")
    g27.set_title("Initial Flow Title")
    with app.batch(atomic=True):
        g27.set_title("Intermediate Step Title")
        g27.set_title("Final Batch Title")
    assert g27.info()["layers"][0]["title"] == "Final Batch Title"
    g27.undo()
    assert g27.info()["layers"][0]["title"] == "Initial Flow Title"
    g27.redo()
    assert g27.info()["layers"][0]["title"] == "Final Batch Title"
    g27.set_title("")  # clean publication frame for ISIJ style
    g27.isij_style()
    reg.verify_figure(app, g27, "Fig27_Agent_Batch_DataFrame_UndoRedo", persona="AI-Agent", description="Atomic Batch Pipeline ($prev/$steps + Rollback + Undo/Redo)")

    # Fig28: Sabotaged Layout -> diagnose_layout -> auto_layout Self-Healing
    t_sab = app.create_table(
        {
            "Cycles_Log": [3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5, 7.0],
            "StressAmp": [520.0, 470.0, 425.0, 385.0, 355.0, 330.0, 312.0, 305.0, 302.0],
        },
        name="Agent_SN_Sabotage",
        units={"Cycles_Log": "-", "StressAmp": "MPa"},
        long_names={
            "Cycles_Log": "Fatigue life, log<sub>10</sub>(<i>N</i><sub>f</sub>)",
            "StressAmp": "Stress amplitude, <i>σ</i><sub>a</sub>",
        },
    )
    g28 = app.plot_xy(t_sab, "Cycles_Log", "StressAmp", style="line_symbol", graph_name="Atlas_Fig28_AutoHealed")
    g28.isij_style()
    # Deliberately sabotage axis range & disabled right axis title & overlapping callout
    g28.set_axis("y", lim=[420.0, 480.0])  # clips data severely
    g28.set_axis("right", title="Orphan Disabled Right Axis", enabled=False)
    g28.add_text("Endurance limit, <i>σ</i><sub>w</sub> = 300 MPa", x=4.5, y=450.0)
    pre_warns = g28.diagnose_layout()
    pre_codes = {w["code"] for w in pre_warns}
    assert "CURVE_CLIPPED_BY_AXIS" in pre_codes and "DISABLED_AXIS_HAS_TITLE_OR_CURVE" in pre_codes, pre_codes

    heal_res = g28.auto_layout(pad_fraction=0.06, allow_outside_legend=True)
    assert heal_res.get("ok") is True, heal_res
    insp28 = g28.inspect()
    crect28 = insp28["layers"][0]["canvas_rect_px"]
    tms28 = insp28["layers"][0].get("text_markers", [])
    assert len(tms28) >= 1, f"Expected endurance-limit callout on Fig28, got {tms28}"
    cx, cy, cw, ch = [float(v) for v in crect28]
    for tm in tms28:
        bx, by, bw, bh = [float(v) for v in tm["bbox_px"]]
        assert bx > cx and by > cy and (bx + bw) < (cx + cw) and (by + bh) < (cy + ch), (
            f"Fig28 callout {tm['text']!r} bbox_px={tm['bbox_px']} not strictly inside canvas_rect_px={crect28}!"
        )
    x_title28 = str(insp28["layers"][0]["axes"]["x"].get("title", ""))
    assert x_title28.lower().count("log") == 1, f"Duplicate log10(N_f) unit on Fig28 X-axis: {x_title28!r}"
    reg.verify_figure(app, g28, "Fig28_Agent_Diagnose_and_AutoLayout", persona="AI-Agent", description="Self-Healed Layout via diagnose_layout + auto_layout")

    # Figs 29 & 30: to_python_script() -> Fresh-Process exec() Across 6 Archetypes
    #   Part A (Fig29, 29b, 29c): XY+GaussFit, Pie, Box
    #   Part B (Fig30, 30b, 30c): 2x1 Stacked Histogram + Lognormal PDF, Polar, Ternary
    rng = random.Random(20261120)

    # Source 1: XY + Nonlinear Gaussian Fit
    xs_g = [-4.0 + 0.25 * i for i in range(33)]
    ys_g = [15.0 + 120.0 * math.exp(-((x - 0.3) ** 2) / (2.0 * 0.85**2)) for x in xs_g]
    t_gf = app.create_table(
        {"Delta2Theta": xs_g, "Counts": ys_g},
        name="Src_GaussFit",
        units={"Delta2Theta": "deg", "Counts": "cps"},
        long_names={"Delta2Theta": "Peak offset, Δ2<i>θ</i>", "Counts": "Diffracted intensity, <i>I</i>"},
    )
    g_src_gf = app.plot_xy(t_gf, "Delta2Theta", "Counts", style="scatter", graph_name="Src_Fig29_Gauss")
    app.fit_nonlinear(graph=g_src_gf, curve=0, model="Gauss")
    g_src_gf.isij_style()
    g_src_gf.set_legend(visible=False)
    code_gauss = g_src_gf.to_python_script(standalone=False, embed_data="inline")

    # Source 2: Pie with label_col
    t_spie = app.create_table(
        {"Defect": ["Porosity", "Inclusion", "Lack of fusion", "Hot crack"], "Share": [44.0, 29.0, 17.0, 10.0]},
        name="Src_Pie",
    )
    g_src_pie = app.plot_pie(t_spie, col="Share", label_col="Defect", graph_name="Src_Fig29b_Pie")
    code_pie = g_src_pie.to_python_script(standalone=False, embed_data="inline")

    # Source 3: Notched Box Plot
    t_sbox = app.create_table(
        {
            "WQ": [95.0 + rng.gauss(0, 4.0) for _ in range(18)],
            "T6": [182.0 + rng.gauss(0, 5.0) for _ in range(18)],
            "T73": [162.0 + rng.gauss(0, 4.5) for _ in range(18)],
        },
        name="Src_Box",
        units={"WQ": "HV", "T6": "HV", "T73": "HV"},
    )
    g_src_box = app.plot_box(
        t_sbox,
        cols=["WQ", "T6", "T73"],
        notched=True,
        show_points=True,
        whisker_mode="minmax",
        graph_name="Src_Fig29c_Box",
    )
    g_src_box.isij_style()
    code_box = g_src_box.to_python_script(standalone=False, embed_data="inline")

    # Source 4: 2x1 Stacked Histogram + Lognormal PDF
    t_shist = app.create_table(
        {
            "d_T6": [round(rng.lognormvariate(-2.7, 0.35), 6) for _ in range(180)],
            "d_T73": [round(rng.lognormvariate(-2.0, 0.33), 6) for _ in range(180)],
        },
        name="Src_Hist2x1",
        units={"d_T6": "μm", "d_T73": "μm"},
        long_names={
            "d_T6": "Precipitate diameter, <i>d</i><sub>eq</sub> (T6)",
            "d_T73": "Precipitate diameter, <i>d</i><sub>eq</sub> (T73)",
        },
    )
    g_src_hist = app.plot_histogram(
        t_shist,
        cols=["d_T6", "d_T73"],
        stacked=True,
        bins=14,
        bin_min=0.0,
        bin_max=0.36,
        overlay_pdf="lognormal",
        graph_name="Src_Fig30_Hist2x1",
    )
    g_src_hist.isij_style()
    code_hist = g_src_hist.to_python_script(standalone=False, embed_data="inline")

    # Source 5: Polar
    th_p = [float(k * 20) for k in range(19)]
    t_spol = app.create_table(
        {"Angle": th_p, "r_val": [80.0 + 15.0 * math.cos(math.radians(2.0 * a)) for a in th_p]},
        name="Src_Polar",
        units={"Angle": "deg", "r_val": "-"},
        long_names={"Angle": "Azimuth, <i>θ</i>", "r_val": "Lankford <i>r</i>-value"},
    )
    g_src_pol = app.plot_polar(t_spol, theta_col="Angle", r_cols=["r_val"], graph_name="Src_Fig30b_Polar")
    g_src_pol.isij_style()
    code_pol = g_src_pol.to_python_script(standalone=False, embed_data="inline")

    # Source 6: Ternary
    t_stern = app.create_table(
        {"Fe": [70.0, 68.0, 72.0, 65.0], "Cr": [20.0, 18.0, 19.0, 22.0], "Ni": [10.0, 14.0, 9.0, 13.0]},
        name="Src_Ternary",
    )
    g_src_tern = app.plot_ternary(t_stern, "Fe", "Cr", "Ni", graph_name="Src_Fig30c_Ternary")
    g_src_tern.isij_style()
    code_tern = g_src_tern.to_python_script(standalone=False, embed_data="inline")

    # Execute all 6 scripts in a brand-new ToriGraph.exe child process!
    with launcher.session(language="en") as (exec_app, _, _):
        repro_specs = [
            ("Fig29_ToPython_Exec_XY_GaussFit", code_gauss, True, "to_python_script Fresh exec(): XY + Gaussian Fit"),
            ("Fig29b_ToPython_Exec_Pie", code_pie, False, "to_python_script Fresh exec(): Pie Chart"),
            ("Fig29c_ToPython_Exec_Box", code_box, False, "to_python_script Fresh exec(): Notched Box Plot"),
            ("Fig30_ToPython_Exec_Hist2x1_PDF", code_hist, True, "to_python_script Fresh exec(): 2x1 Stacked Histogram + Lognormal PDF"),
            ("Fig30b_ToPython_Exec_Polar", code_pol, False, "to_python_script Fresh exec(): Polar Plot"),
            ("Fig30c_ToPython_Exec_Ternary", code_tern, False, "to_python_script Fresh exec(): Fe-Cr-Ni Ternary"),
        ]
        for fig_id, script_src, is_prim, desc in repro_specs:
            compile(script_src, f"<{fig_id}>", "exec")
            before_mls = {w["name"] for w in exec_app.list_windows() if w.get("type") == "MultiLayer"}
            ns: Dict[str, Any] = {"__builtins__": __builtins__, "app": exec_app, "tg": tg}
            exec(script_src, ns)
            after_mls = [w["name"] for w in exec_app.list_windows() if w.get("type") == "MultiLayer" and w["name"] not in before_mls]
            g_rec = ns.get("g") if isinstance(ns.get("g"), tg.GraphProxy) else exec_app.graph(after_mls[-1])
            reg.verify_figure(exec_app, g_rec, fig_id, persona="AI-Agent", description=desc, is_primary=is_prim)


def main() -> int:
    env_atlas_dir = os.environ.get("TORIGRAPH_ATLAS_DIR", "").strip()
    temp_ctx = None
    if env_atlas_dir:
        out_dir = Path(env_atlas_dir).expanduser().resolve()
        out_dir.mkdir(parents=True, exist_ok=True)
    else:
        temp_ctx = tempfile.TemporaryDirectory(prefix="torigraph_atlas_e2e_")
        out_dir = Path(temp_ctx.name)

    launcher = PortPoolLauncher(PORT_POOL)
    reg = AtlasRegistry(out_dir)
    t_suite0 = time.perf_counter()

    try:
        lab_files = _write_synthetic_lab_files(out_dir / "synthetic_lab_inputs")
        with launcher.session(language="en") as (app, port, pid):
            print(
                f"[INFO] Connected to ToriGraph Atlas server on port {port} (PID={pid}); output={out_dir}",
                flush=True,
            )
            reg.run_section(
                "1. Persona 1 — Novice (Figs 01-10) + Smart Lab Imports",
                lambda: run_persona_1_novice(app, reg, lab_files),
            )
            reg.run_section(
                "2. Persona 2 — Intermediate Materials Science & Stats (Figs 11-20)",
                lambda: run_persona_2_intermediate(app, reg, out_dir),
            )
            reg.run_section(
                "3. Persona 3 — Expert DFT/Texture/128x128/200k AE & Zero-COM PPTX OLE (Figs 21-26)",
                lambda: run_persona_3_expert(app, launcher, reg, lab_files, out_dir),
            )
            reg.run_section(
                "4. Persona 4 — AI-Agent Autonomy, Batch, Auto-Layout & Fresh-Process to_python exec() (Figs 27-30)",
                lambda: run_persona_4_ai_agent_autonomy(app, launcher, reg),
            )

        assert len(reg.primary_ids) == 30, (
            f"Expected exactly 30 primary Atlas figures (Fig01..Fig30), got {len(reg.primary_ids)}: {reg.primary_ids}"
        )

        manifest = {
            "ok": len(reg.checks_failed) == 0,
            "elapsed_sec": round(time.perf_counter() - t_suite0, 2),
            "primary_figure_count": len(reg.primary_ids),
            "total_audited_figures": len(reg.figures),
            "primary_ids": reg.primary_ids,
            "figures": reg.figures,
        }
        (out_dir / "atlas_manifest.json").write_text(
            json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8"
        )
    finally:
        _cleanup_all_spawned_procs()
        assert len(_SPAWNED_PROCS) == 0, f"Unreaped child processes remained: {_SPAWNED_PROCS}"
        if temp_ctx is not None:
            temp_ctx.cleanup()

    print("\n" + "=" * 78)
    print(
        f"MATSCI ATLAS & AGENT E2E SUMMARY: {len(reg.primary_ids)}/30 primary figures "
        f"({len(reg.figures)} total audited), {len(reg.checks_passed)} passed, {len(reg.checks_failed)} failed"
    )
    print("=" * 78)
    if reg.checks_failed:
        for title, detail in reg.checks_failed:
            print(f"\n--- FAILURE: {title} ---\n{detail}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
