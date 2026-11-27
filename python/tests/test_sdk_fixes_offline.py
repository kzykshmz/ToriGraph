"""Offline unit tests for SDK client fixes (mock transport only, no ToriGraph process)."""

from __future__ import annotations

import json
import sys
from collections import deque
from pathlib import Path
from typing import Any, Deque, Dict, List, Optional, Tuple

import pytest

import torigraph as tg
import torigraph.client as tg_client
import torigraph.pptx as tg_pptx
from torigraph.client import (
    BatchReadError,
    GraphProxy,
    RpcError,
    Session,
    ToriGraphDisconnectedError,
    ToriGraphTimeoutError,
    TransportError,
    WorkbookProxy,
    _resolve_tcp_auth_token,
)


class MockTransport:
    """Queue-backed mock transport supporting scripted replies and exception injection."""

    endpoint = "fake://mock"

    def __init__(self, replies: Optional[List[Any]] = None) -> None:
        self.replies: Deque[Any] = deque(replies or [])
        self.sent: List[Tuple[Dict[str, Any], Optional[float]]] = []
        self.closed = False

    def send_line(self, payload: bytes, timeout: Optional[float] = None) -> None:
        if self.closed:
            raise TransportError("Transport is closed.")
        self.sent.append((json.loads(payload.decode("utf-8")), timeout))

    def recv_line(self, timeout: Optional[float] = None) -> bytes:
        if self.closed:
            raise TransportError("Connection closed by ToriGraph (fake://mock).")
        if not self.replies:
            raise TransportError(f"Timed out after {timeout:.1f}s waiting for a response.")
        item = self.replies.popleft()
        if isinstance(item, BaseException):
            raise item
        if callable(item):
            item = item(self.sent[-1][0])
        return json.dumps(item).encode("utf-8") if not isinstance(item, bytes) else item

    def close(self) -> None:
        self.closed = True


def _ok(result: Any) -> Any:
    return lambda req: {"jsonrpc": "2.0", "id": req["id"], "result": result}


def test_client_imports_sys() -> None:
    """E2-1: client.py must import sys so Windows _discover_torigraph_executable works."""
    assert "sys" in vars(tg_client)
    assert tg_client.sys is sys


def test_discover_torigraph_executable_without_env_or_build(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """E2-1: _discover_torigraph_executable raises ToriGraphError (never NameError) when absent."""
    monkeypatch.delenv("TORIGRAPH_EXE", raising=False)
    fake_client = tmp_path / "pkg" / "torigraph" / "client.py"
    fake_client.parent.mkdir(parents=True)
    fake_client.write_text("", encoding="utf-8")
    monkeypatch.setattr(tg_client, "__file__", str(fake_client))
    monkeypatch.setattr(tg_client.shutil, "which", lambda _name: None)
    monkeypatch.setenv("LOCALAPPDATA", str(tmp_path / "none1"))
    monkeypatch.setenv("ProgramFiles", str(tmp_path / "none2"))
    monkeypatch.setenv("ProgramFiles(x86)", str(tmp_path / "none3"))
    if tg_client.sys.platform == "win32":
        import types

        fake_winreg = types.SimpleNamespace(
            HKEY_CURRENT_USER=1,
            HKEY_LOCAL_MACHINE=2,
            OpenKey=lambda *args, **kwargs: (_ for _ in ()).throw(OSError("not found")),
        )
        monkeypatch.setitem(sys.modules, "winreg", fake_winreg)
    with pytest.raises(tg_client.ToriGraphError, match="Could not find ToriGraph executable"):
        tg_client._discover_torigraph_executable()


def test_tcp_token_resolution_order_explicit_env_file(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """C5-#9: explicit arg > query token > env TORIGRAPH_RPC_TOKEN > token file."""
    if tg_client.os.name == "nt":
        monkeypatch.setenv("LOCALAPPDATA", str(tmp_path))
        tok_dir = tmp_path / "ToriGraph" / "rpc"
    else:
        monkeypatch.setenv("XDG_DATA_HOME", str(tmp_path))
        tok_dir = tmp_path / "ToriGraph" / "rpc"
    tok_dir.mkdir(parents=True)
    (tok_dir / "tcp_18900.token").write_text("stale-file-token\n", encoding="utf-8")

    # 1. File token used when neither explicit nor env is set
    monkeypatch.delenv("TORIGRAPH_RPC_TOKEN", raising=False)
    assert _resolve_tcp_auth_token("127.0.0.1", 18900) == "stale-file-token"

    # 2. Env var overrides stale token file
    monkeypatch.setenv("TORIGRAPH_RPC_TOKEN", "env-token")
    assert _resolve_tcp_auth_token("127.0.0.1", 18900) == "env-token"

    # 3. Query token overrides env var
    assert _resolve_tcp_auth_token("127.0.0.1", 18900, query_token="query-token") == "query-token"

    # 4. Explicit arg overrides everything
    assert (
        _resolve_tcp_auth_token(
            "127.0.0.1", 18900, explicit_token="explicit-token", query_token="query-token"
        )
        == "explicit-token"
    )


def test_create_table_list_designations_converted_to_dict() -> None:
    """E2-5: create_table(designations=[...]) sends {col: role} dict to table.set_data_dict."""
    t = MockTransport([_ok({"name": "Data1"}), _ok({"ok": True})])
    s = Session(t)
    tbl = s.create_table(
        {"Xval": [1.0, 2.0], "Yval": [10.0, 20.0], "Err": [0.5, 0.8]},
        designations=["X", "Y", "yErr"],
    )
    assert tbl.name == "Data1"
    assert [req["method"] for req, _ in t.sent] == ["table.create", "table.set_data_dict"]
    set_params = t.sent[1][0]["params"]
    assert set_params["designations"] == {"Xval": "X", "Yval": "Y", "Err": "yErr"}

    # Too many designations raises ValueError before issuing RPCs
    t2 = MockTransport([])
    s2 = Session(t2)
    with pytest.raises(ValueError, match="designations has 4 entries"):
        s2.create_table({"A": [1.0], "B": [2.0]}, designations=["X", "Y", "yErr", "Z"])
    assert t2.sent == []

    # TableProxy.set_data_dict also accepts a list of designations
    t3 = MockTransport([_ok({"ok": True})])
    s3 = Session(t3)
    s3.table("Data1").set_data_dict(
        {"A": [1.0], "B": [2.0], "E": [0.1]}, designations=["X", "Y", "yErr"]
    )
    assert t3.sent[0][0]["params"]["designations"] == {"A": "X", "B": "Y", "E": "yErr"}


def test_rpc_error_message_includes_first_10_available_candidates() -> None:
    """E2-7: RpcError.__str__ formats up to 10 available candidates + overflow count."""
    cands = [f"pal_{i}" for i in range(13)]
    err = RpcError(-32602, "Unknown palette", {"available": cands})
    msg = str(err)
    assert err.available == cands
    assert "pal_0" in msg and "pal_9" in msg
    assert "pal_10" not in msg
    assert "+3 more" in msg

    small = RpcError(-32602, "Unknown palette", {"available": ["Viridis", "Plasma"]})
    assert "available: Viridis, Plasma)" in str(small)
    assert "more" not in str(small)


def test_connect_missing_endpoint_gives_actionable_guidance(monkeypatch: pytest.MonkeyPatch) -> None:
    """E2-7: tg.connect() without endpoint mentions pipe:// and connect_or_launch()."""
    monkeypatch.delenv("TORIGRAPH_RPC_ENDPOINT", raising=False)
    with pytest.raises(TransportError, match=r"pipe://.*connect_or_launch\(\)"):
        tg.connect()


def test_batch_context_rejects_synchronous_reads() -> None:
    """C5-#16: synchronous read calls inside `with app.batch():` raise BatchReadError."""
    t = MockTransport([_ok({"ok": True, "completed": 3, "total": 3, "results": [{"name": "T1"}, {"ok": True}, {"name": "G1"}]})])
    s = Session(t)

    with s.batch() as b:
        tbl = s.create_table(rows=3, cols=2)
        tbl.set_column("A", [1.0, 2.0, 3.0])
        g = s.plot_xy(tbl, "A", ["B"])

        for read_call in (
            lambda: tbl.info(),
            lambda: tbl.get_column("A"),
            lambda: tbl.get_cell(0, "A"),
            lambda: tbl.get_meta("A"),
            lambda: tbl.get_masked_rows("A"),
            lambda: tbl.stats("A"),
            lambda: tbl.get_data_dict(),
            lambda: tbl.to_dataframe(),
            lambda: g.info(),
            lambda: g.inspect(),
            lambda: g.diagnose_layout(),
            lambda: g.to_python(),
            lambda: g.to_python_script(),
            lambda: s.matrix("M1").info(),
            lambda: s.matrix("M1").get_data(),
            lambda: s.info(),
            lambda: s.list_windows(),
            lambda: s.window_info("T1"),
            lambda: s.describe_api(),
            lambda: s.list_workbooks(),
            lambda: s.workbook("WB1").info(),
            lambda: s.workbook("WB1").sheets(),
        ):
            with pytest.raises(BatchReadError):
                read_call()

    assert len(b.commands) == 3
    assert len(t.sent) == 1
    assert t.sent[0][0]["method"] == "app.batch"


def test_lab_slide_style_and_slide_style_tracking() -> None:
    """E2-4: GraphProxy.lab_slide_style() applies lab_slide_83mm and records graph on Session."""
    t = MockTransport([_ok({"ok": True}), _ok({"ok": True})])
    s = Session(t)
    g = s.graph("Graph1")

    g.lab_slide_style()
    assert t.sent[0][0]["method"] == "graph.apply_style_preset"
    assert t.sent[0][0]["params"] == {"graph": "Graph1", "preset": "lab_slide_83mm"}
    assert "Graph1" in s._slide_preset_graphs

    g.slide_style()
    assert t.sent[1][0]["params"] == {"graph": "Graph1", "preset": "slide_28pt"}
    assert "Graph1" not in s._slide_preset_graphs
    assert "lab_slide_style" in (GraphProxy.slide_style.__doc__ or "")


def test_plot_methods_return_graph_proxy_consistently() -> None:
    """E2-6: plot_histogram_cumulative, plot_density2d, plot_pole_figure(plot=True) return GraphProxy."""
    t = MockTransport(
        [
            _ok({"graph": "HistG", "bin_table": "Bins1"}),
            _ok({"graph": "DensG", "matrix": "Mat1"}),
            _ok({"matrix": "Mat2"}),  # plot=False
            _ok({"graph": "PoleG", "matrix": "PoleMat"}),
            _ok({"matrix": "PoleMat2"}),  # plot=False
        ]
    )
    s = Session(t)

    g1 = s.plot_histogram_cumulative("T1", "Val")
    assert isinstance(g1, GraphProxy) and g1.name == "HistG"
    assert g1.result["bin_table"] == "Bins1"  # type: ignore[attr-defined]

    g2 = s.plot_density2d("T1", "X", "Y")
    assert isinstance(g2, GraphProxy) and g2.name == "DensG"
    assert g2.result["matrix"] == "Mat1"  # type: ignore[attr-defined]

    d_only = s.plot_density2d("T1", "X", "Y", plot=False)
    assert isinstance(d_only, dict) and d_only["matrix"] == "Mat2"

    g3 = s.plot_pole_figure("T1", "A", "B", "I", plot=True)
    assert isinstance(g3, GraphProxy) and g3.name == "PoleG"

    p_only = s.plot_pole_figure("T1", "A", "B", "I", plot=False)
    assert isinstance(p_only, dict) and p_only["matrix"] == "PoleMat2"


def test_session_workbook_domain() -> None:
    """E2-8: Session.workbook exposes WorkbookProxy and dotted workbook.* calls."""
    t = MockTransport([_ok({"name": "Book1"}), _ok({"workbooks": [{"name": "Book1", "sheets": []}]})])
    s = Session(t)
    wb = s.workbook("Book1")
    assert isinstance(wb, WorkbookProxy) and wb.name == "Book1"

    s.workbook.create(name="Book1", sheets=2)
    assert t.sent[0][0]["method"] == "workbook.create"
    assert wb.info()["name"] == "Book1"
    assert t.sent[1][0]["method"] == "workbook.list"


def test_timeout_vs_disconnect_and_reconnect(monkeypatch: pytest.MonkeyPatch) -> None:
    """C5-#18: distinguish ToriGraphTimeoutError vs ToriGraphDisconnectedError and reconnect()."""
    t1 = MockTransport([_ok({"pong": True})])  # initial ping during connect()
    t2 = MockTransport([_ok({"pong": True}), _ok({"version": tg.__version__})])

    transports = deque([t1, t2])
    monkeypatch.setattr(
        tg_client,
        "_parse_and_connect_transport",
        lambda ep, timeout=30.0, token=None: transports.popleft(),
    )

    s = tg.connect("pipe://test_pipe", check_version=False)
    tbl = s.table("Data1")

    # 1. Timeout raises ToriGraphTimeoutError (which is a TransportError, not DisconnectedError)
    with pytest.raises(ToriGraphTimeoutError) as ei_to:
        s.call("analysis.slow", rpc_timeout=0.5)
    assert isinstance(ei_to.value, TransportError)
    assert not isinstance(ei_to.value, ToriGraphDisconnectedError)
    assert not s.usable

    # Calling while broken raises ToriGraphDisconnectedError
    with pytest.raises(ToriGraphDisconnectedError):
        s.call("ping")

    # 2. Reconnect restores usability without losing existing proxy handles
    assert s.reconnect() is s
    assert s.usable
    res = tbl.info()  # consumes second reply on t2
    assert res["version"] == tg.__version__
    assert t2.sent[-1][0]["params"] == {"table": "Data1"}

    # 3. Real disconnect raises ToriGraphDisconnectedError (not TimeoutError)
    t2.replies.append(TransportError("Connection closed by ToriGraph (pipe://test_pipe)."))
    with pytest.raises(ToriGraphDisconnectedError) as ei_dc:
        s.call("ping")
    assert isinstance(ei_dc.value, TransportError)
    assert not isinstance(ei_dc.value, ToriGraphTimeoutError)
    assert not s.usable

    # 4. Session without _reconnect_info cannot reconnect
    custom = Session(MockTransport([]))
    with pytest.raises(ToriGraphDisconnectedError, match="custom transport"):
        custom.reconnect()


def test_batch_read_error_on_new_readonly_rpcs_and_pptx(tmp_path: Path) -> None:
    """BatchReadError must fire on row_stats, determinant, check_updates, and pptx reads."""
    t = MockTransport([])
    s = Session(t)
    tbl = s.table("Data1")
    mat = s.matrix("Matrix1")

    with s.batch():
        with pytest.raises(BatchReadError):
            tbl.row_stats()
        with pytest.raises(BatchReadError):
            mat.determinant()
        with pytest.raises(BatchReadError):
            s.check_updates()
        with pytest.raises(BatchReadError):
            tg_pptx._graph_info_safe(s, "Graph1")
        with pytest.raises(BatchReadError):
            tg_pptx.export_graphs_to_pptx(s, tmp_path / "batch_guard.pptx", ["Graph1"])

    assert s._active_batch is None
    assert t.sent == []


def test_batch_nested_contexts_and_exception_unwinding() -> None:
    """Nested with app.batch(): restores outer context on exit and on exceptions."""
    t = MockTransport(
        [
            _ok({"ok": True, "completed": 1, "total": 1, "rolled_back": False, "results": [{"ok": True}]}),
            _ok({"ok": True, "completed": 2, "total": 2, "rolled_back": False, "results": [{"ok": True}, {"ok": True}]}),
            _ok({"pong": True}),
        ]
    )
    s = Session(t)

    with s.batch() as outer:
        s.table("T1").set_cell(0, 0, 1.0)
        with s.batch() as inner:
            s.table("T1").set_cell(1, 0, 2.0)
            assert s._active_batch is inner
        assert s._active_batch is outer
        s.table("T1").set_cell(2, 0, 3.0)

    assert s._active_batch is None
    assert len(t.sent) == 2
    assert len(t.sent[0][0]["params"]["commands"]) == 1
    assert len(t.sent[1][0]["params"]["commands"]) == 2

    with pytest.raises(BatchReadError):
        with s.batch() as outer2:
            s.table("T1").set_cell(0, 0, 10.0)
            with s.batch():
                s.table("T1").set_cell(1, 0, 20.0)
                s.list_windows()
            assert False, "unreachable"  # pragma: no cover

    assert s._active_batch is None
    assert s.ping()["pong"] is True


def test_late_response_discarded_and_future_id_rejected() -> None:
    """Stale responses with id < req_id are discarded; higher unmatched ids invalidate."""
    t = MockTransport(
        [
            {"jsonrpc": "2.0", "id": 0, "result": {"stale": True}},
            _ok({"pong": True}),
            {"jsonrpc": "2.0", "id": 999, "result": {"wrong_future": True}},
        ]
    )
    s = Session(t)

    res = s.ping()
    assert res == {"pong": True}
    assert s.discarded_responses == 1
    assert s.usable

    with pytest.raises(TransportError, match="while waiting for id"):
        s.ping()
    assert not s.usable


def test_reconnect_failure_paths(monkeypatch: pytest.MonkeyPatch) -> None:
    """reconnect() handles failed transport open, dead subprocess, and closed sessions."""
    t1 = MockTransport([_ok({"pong": True})])
    calls = 0

    def fake_connect(ep: str, timeout: float = 30.0, token: Optional[str] = None) -> MockTransport:
        nonlocal calls
        calls += 1
        if calls == 1:
            return t1
        raise TransportError("connection refused")

    monkeypatch.setattr(tg_client, "_parse_and_connect_transport", fake_connect)
    s = tg.connect("pipe://test_fail_reconnect", check_version=False)

    with pytest.raises(ToriGraphDisconnectedError, match="Reconnect to pipe://test_fail_reconnect failed"):
        s.reconnect()

    class DeadProc:
        returncode = 17

        def poll(self) -> int:
            return self.returncode

    s._process = DeadProc()  # type: ignore[assignment]
    with pytest.raises(ToriGraphDisconnectedError, match="exited \\(code 17\\)"):
        s.reconnect()

    s._process = None
    s.close()
    with pytest.raises(ToriGraphDisconnectedError, match="already closed"):
        s.reconnect()


def test_reconnect_reresolves_tcp_auth_token(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Reconnecting a tokenless TCP session re-reads the updated token file instead of reusing stale auth."""
    monkeypatch.delenv("TORIGRAPH_RPC_TOKEN", raising=False)
    if tg_client.os.name == "nt":
        monkeypatch.setenv("LOCALAPPDATA", str(tmp_path / "local"))
    else:
        monkeypatch.delenv("LOCALAPPDATA", raising=False)
        monkeypatch.setenv("XDG_DATA_HOME", str(tmp_path / "local"))
    token_dir = tmp_path / "local" / "ToriGraph" / "rpc"
    token_dir.mkdir(parents=True)
    token_file = token_dir / "tcp_45123.token"
    token_file.write_text("old_server_token_aaa", encoding="utf-8")

    observed_tokens: List[Optional[str]] = []
    t1 = MockTransport([_ok({"pong": True})])
    t2 = MockTransport([_ok({"pong": True}), _ok({"pong": True})])
    transports = deque([t1, t2])

    def fake_parse(ep: str, timeout: float = 30.0, token: Optional[str] = None) -> MockTransport:
        # Simulate _TcpTransport resolving auth fresh via _resolve_tcp_auth_token when token is None
        host, port_s = ep.replace("tcp://", "").split(":")
        resolved = _resolve_tcp_auth_token(host, int(port_s), explicit_token=token)
        observed_tokens.append(resolved)
        return transports.popleft()

    monkeypatch.setattr(tg_client, "_parse_and_connect_transport", fake_parse)
    s = tg.connect("tcp://127.0.0.1:45123", check_version=False)
    assert observed_tokens == ["old_server_token_aaa"]
    assert s._reconnect_info is not None and s._reconnect_info[2] is None

    # Server restarts and writes a brand new random token file
    token_file.write_text("new_server_token_bbb", encoding="utf-8")
    s.reconnect()
    assert observed_tokens == ["old_server_token_aaa", "new_server_token_bbb"]
    assert s.ping()["pong"] is True


def test_rpc_busy_32003_retry_backoff(monkeypatch: pytest.MonkeyPatch) -> None:
    """-32003 (re-entrant busy) retries up to 3 times with 0.05*(attempt+1) backoff."""
    sleeps: List[float] = []
    monkeypatch.setattr(tg_client.time, "sleep", lambda sec: sleeps.append(float(sec)))

    def busy(req: Dict[str, Any]) -> Dict[str, Any]:
        return {
            "jsonrpc": "2.0",
            "id": req["id"],
            "error": {"code": -32003, "message": "Server busy (re-entrant RPC)"},
        }

    # 1. Succeeds on 4th attempt (after 3 retries)
    t_ok = MockTransport([busy, busy, busy, _ok({"pong": True})])
    s_ok = Session(t_ok)
    assert s_ok.ping() == {"pong": True}
    assert len(t_ok.sent) == 4
    assert sleeps == pytest.approx([0.05, 0.10, 0.15])

    # 2. Fails after exhausting 3 retries (4 attempts total)
    sleeps.clear()
    t_fail = MockTransport([busy, busy, busy, busy])
    s_fail = Session(t_fail)
    with pytest.raises(RpcError) as ei_busy:
        s_fail.ping()
    assert ei_busy.value.code == -32003
    assert len(t_fail.sent) == 4
    assert sleeps == pytest.approx([0.05, 0.10, 0.15])

    # 3. Non--32003 RPC error raises immediately with zero retries
    sleeps.clear()
    t_other = MockTransport(
        [
            lambda req: {
                "jsonrpc": "2.0",
                "id": req["id"],
                "error": {"code": -32602, "message": "Invalid params"},
            }
        ]
    )
    s_other = Session(t_other)
    with pytest.raises(RpcError) as ei_other:
        s_other.ping()
    assert ei_other.value.code == -32602
    assert len(t_other.sent) == 1
    assert sleeps == []


def test_pptx_slide_scale_server_fields_and_close_discard() -> None:
    """New-server applied_style_preset=='' overrides fallback; old servers still fall back."""
    t = MockTransport(
        [
            _ok({"graph": "Graph1", "preset": "lab_slide_83mm"}),
            _ok({"closed": True, "name": "Graph1"}),
        ]
    )
    s = Session(t)
    g = GraphProxy(s, "Graph1")
    g.lab_slide_style()
    assert "Graph1" in s._slide_preset_graphs

    # New server recreated Graph1 without preset -> applied_style_preset='' present -> None
    assert (
        tg_pptx._graph_slide_scale(
            s, "Graph1", {"name": "Graph1", "applied_style_preset": "", "style_preset": ""}
        )
        is None
    )
    # Old server response lacks all slide-preset fields -> session fallback applies
    assert tg_pptx._graph_slide_scale(s, "Graph1", {"name": "Graph1"}) == pytest.approx(1.4)

    s.close_window("Graph1")
    assert "Graph1" not in s._slide_preset_graphs
    assert tg_pptx._graph_slide_scale(s, "Graph1", {"name": "Graph1"}) is None


def test_comp_obj_unicode_lengths_count_characters() -> None:
    """Non-Windows CompObj mirrors ole32!WriteFmtUserTypeStg: names in the ANSI part, then the
    Unicode marker and three empty Unicode entries (no characters-vs-bytes length ambiguity)."""
    import struct

    stream = tg_pptx._build_comp_obj_stream("ToriGraph Graph", "ToriGraph.Graph.1")
    pos = 28
    ansi: List[bytes] = []
    for _ in range(3):
        (n,) = struct.unpack_from("<I", stream, pos)
        ansi.append(stream[pos + 4 : pos + 4 + n])
        pos += 4 + n
    assert ansi == [b"ToriGraph Graph\x00", b"Embed Source\x00", b"ToriGraph.Graph.1\x00"]
    assert stream[pos:] == struct.pack("<IIII", 0x71B239F4, 0, 0, 0)


def test_unix_local_socket_candidates_follow_qdir_temp_path(monkeypatch: pytest.MonkeyPatch) -> None:
    """pipe://name resolves under $TMPDIR (QDir::tempPath) first, then /tmp."""
    import tempfile

    monkeypatch.setattr(tempfile, "gettempdir", lambda: "/var/folders/ab/T/")
    assert tg_client._unix_local_socket_candidates("torigraph_rpc_42") == [
        "/var/folders/ab/T/torigraph_rpc_42",
        "/tmp/torigraph_rpc_42",
    ]
    monkeypatch.setattr(tempfile, "gettempdir", lambda: "/tmp")
    assert tg_client._unix_local_socket_candidates("torigraph_rpc_42") == ["/tmp/torigraph_rpc_42"]
    assert tg_client._unix_local_socket_candidates("/run/user/1000/tg.sock") == ["/run/user/1000/tg.sock"]


def test_local_pipe_transport_unix_tries_tmpdir_then_tmp(monkeypatch: pytest.MonkeyPatch) -> None:
    """The Unix branch connects to $TMPDIR/<name>, falling back to /tmp/<name>."""
    import os as real_os
    import tempfile

    attempts: List[str] = []

    class FakeSock:
        def __init__(self, *_a: Any) -> None:
            self.closed = False

        def settimeout(self, _t: float) -> None:
            pass

        def connect(self, path: str) -> None:
            attempts.append(path)
            if path != "/tmp/torigraph_rpc_7":
                raise FileNotFoundError(2, "No such file or directory")

        def close(self) -> None:
            self.closed = True

    class PosixOs:
        name = "posix"

        def __getattr__(self, key: str) -> Any:
            return getattr(real_os, key)

    monkeypatch.setattr(tg_client, "os", PosixOs())
    monkeypatch.setattr(tg_client.socket, "AF_UNIX", 1, raising=False)
    monkeypatch.setattr(tg_client.socket, "socket", FakeSock)
    monkeypatch.setattr(tempfile, "gettempdir", lambda: "/var/folders/ab/T")

    t = tg_client._LocalPipeTransport("torigraph_rpc_7", timeout=1.0)
    assert attempts == ["/var/folders/ab/T/torigraph_rpc_7", "/tmp/torigraph_rpc_7"]
    assert isinstance(t._unix_sock, FakeSock)

    attempts.clear()
    with pytest.raises(TransportError, match="/var/folders/ab/T/torigraph_rpc_8"):
        tg_client._LocalPipeTransport("torigraph_rpc_8", timeout=1.0)
    assert attempts == ["/var/folders/ab/T/torigraph_rpc_8", "/tmp/torigraph_rpc_8"]


def test_overlay_grid_and_cell_shrink_helpers() -> None:
    """slide= grid picks the smallest grid holding all graphs; >6 raise. Cell shrink keeps aspect."""
    assert tg_pptx._overlay_grid_for_count(1, 1) == (1, 1)
    assert tg_pptx._overlay_grid_for_count(2, 4) == (2, 2)  # per_slide grid holds them
    assert tg_pptx._overlay_grid_for_count(5, 1) == (2, 3)
    assert tg_pptx._overlay_grid_for_count(5, 4) == (2, 3)
    assert tg_pptx._overlay_grid_for_count(6, 2) == (2, 3)
    with pytest.raises(ValueError, match="at most 6"):
        tg_pptx._overlay_grid_for_count(7, 6)

    cell = (1000, 2000, 300_000, 200_000)
    # fits -> unchanged, not reported
    assert tg_pptx._shrink_rect_into_cell_emu(5, 6, 100_000, 100_000, cell) == (5, 6, 100_000, 100_000, None)
    l_e, t_e, w_e, h_e, orig = tg_pptx._shrink_rect_into_cell_emu(0, 0, 400_000, 400_000, cell)
    assert w_e <= cell[2] and h_e <= cell[3] and w_e == h_e == 200_000
    assert l_e == 1000 + 50_000 and t_e == 2000
    assert orig == (round(400_000 / tg_pptx.EMU_PER_CM, 2),) * 2


def test_fit_nonlinear_fixed_dict_str_and_list() -> None:
    t = MockTransport([_ok({"ok": True}), _ok({"ok": True}), _ok({"ok": True})])
    s = Session(t)
    s.fit_nonlinear(model="Gauss", fixed={"y0": True, "xc": 0})
    s.fit_nonlinear(model="Gauss", fixed="y0")
    s.fit_nonlinear(model="Gauss", fixed=["y0", "xc"])
    assert t.sent[0][0]["params"]["fixed"] == {"y0": True, "xc": False}
    assert t.sent[1][0]["params"]["fixed"] == ["y0"]
    assert t.sent[2][0]["params"]["fixed"] == ["y0", "xc"]


def test_graph_move_curve_named_curve_to_int_index() -> None:
    t = MockTransport([_ok({"ok": True}), _ok({"ok": True}), _ok({"ok": True}), _ok({"ok": True})])
    s = Session(t)
    g = s.graph("Graph1")
    g.move_curve("Table1_Y2", to_index=0)
    g.move_curve("Table1_Y2", 0)
    g.move_curve(1, 0)
    g.move_curve("Table1_Y2", "top")
    assert t.sent[0][0]["params"] == {"graph": "Graph1", "curve": "Table1_Y2", "to_index": 0}
    assert t.sent[1][0]["params"] == {"graph": "Graph1", "curve": "Table1_Y2", "to_index": 0}
    assert t.sent[2][0]["params"] == {"graph": "Graph1", "from_index": 1, "to_index": 0}
    assert t.sent[3][0]["params"] == {"graph": "Graph1", "curve": "Table1_Y2", "direction": "top"}


def test_workbook_proxy_batch_registration_and_sanitize() -> None:
    t = MockTransport(
        [
            _ok(
                {
                    "ok": True,
                    "completed": 3,
                    "total": 3,
                    "rolled_back": False,
                    "results": [
                        {"ok": True, "name": "Book1"},
                        {"ok": True, "name": "Specimens"},
                        {"ok": True, "table": "Specimens_S2"},
                    ],
                }
            )
        ]
    )
    s = Session(t)
    with s.batch():
        wb = s.new_workbook("Book1")
        wb.rename("Specimens")
        sh = wb.add_sheet("S2")
    assert wb.name == "Specimens"
    assert sh.name == "Specimens_S2"
    cmds = t.sent[0][0]["params"]["commands"]
    assert cmds[1]["params"]["book"] == "$steps[0].name"
    assert cmds[2]["params"]["book"] == "$steps[1].name"
    assert tg_client._sanitize_for_json({"wb": wb}) == {"wb": "Specimens"}


def test_single_str_and_int_column_coercion() -> None:
    t = MockTransport([_ok({"ok": True}) for _ in range(11)])
    s = Session(t)
    s.clustering("T1", "ColA", k=2)
    s.clustering("T1", 0, k=2)
    s.fit_mlr("T1", "ColX", "ColY")
    s.fit_logistic("T1", y_col="Resp", x_cols="Dose")
    s.batch_peaks("T1", "TwoTheta", "Sample1")
    s.average_curves("G1", curves="CurveA")
    s.anova_oneway("T1", "GroupA")
    s.nonparametric("T1", "GroupA")
    s.pca("T1", "FeatA")
    s.correlation("T1", "FeatA")
    s.clustering("T1", ["ColA", "ColB"], k=2)

    assert t.sent[0][0]["params"]["cols"] == ["ColA"]
    assert t.sent[1][0]["params"]["cols"] == [0]
    assert t.sent[2][0]["params"]["x_cols"] == ["ColX"]
    assert t.sent[3][0]["params"]["x_cols"] == ["Dose"]
    assert t.sent[4][0]["params"]["y_cols"] == ["Sample1"]
    assert t.sent[5][0]["params"]["curves"] == ["CurveA"]
    assert t.sent[6][0]["params"]["cols"] == ["GroupA"]
    assert t.sent[7][0]["params"]["cols"] == ["GroupA"]
    assert t.sent[8][0]["params"]["cols"] == ["FeatA"]
    assert t.sent[9][0]["params"]["cols"] == ["FeatA"]
    assert t.sent[10][0]["params"]["cols"] == ["ColA", "ColB"]


def test_coerce_tabular_input_col_names_rename_reorder_subset() -> None:
    import numpy as np

    # Dict renaming
    cols, order, *_ = tg_client._coerce_tabular_input(
        {"a": [1, 2], "b": [3, 4]}, col_names=["X", "Y"]
    )
    assert order == ["X", "Y"]
    assert list(cols.keys()) == ["X", "Y"]
    assert cols["X"] == [1, 2] and cols["Y"] == [3, 4]

    # Dict partial-overlap rename (e.g., ["b", "X"] must NOT leave stray "a" key)
    cols2, order2, *_ = tg_client._coerce_tabular_input(
        {"a": [1, 2], "b": [3, 4]}, col_names=["b", "X"]
    )
    assert order2 == ["b", "X"]
    assert list(cols2.keys()) == ["b", "X"]
    assert cols2["b"] == [1, 2] and cols2["X"] == [3, 4]

    # Dict subset & reorder
    cols3, order3, *_ = tg_client._coerce_tabular_input(
        {"a": [1, 2], "b": [3, 4], "c": [5, 6]}, col_names=["c", "a"]
    )
    assert order3 == ["c", "a"]
    assert list(cols3.keys()) == ["c", "a"]
    assert cols3["c"] == [5, 6] and cols3["a"] == [1, 2]

    # Structured numpy ndarray renaming and subsetting
    arr = np.array([(1.0, 10.0, 100.0), (2.0, 20.0, 200.0)], dtype=[("t", "f8"), ("v", "f8"), ("w", "f8")])
    scols, sorder, *_ = tg_client._coerce_tabular_input(arr, col_names=["Time", "Signal", "Weight"])
    assert sorder == ["Time", "Signal", "Weight"]
    assert list(scols.keys()) == ["Time", "Signal", "Weight"]
    assert scols["Signal"] == [10.0, 20.0]

    scols_sub, sorder_sub, *_ = tg_client._coerce_tabular_input(arr, col_names=["w", "t"])
    assert sorder_sub == ["w", "t"]
    assert list(scols_sub.keys()) == ["w", "t"]
    assert scols_sub["w"] == [100.0, 200.0]

    # Length mismatch with non-existing name raises ValueError
    with pytest.raises(ValueError, match="col_names"):
        tg_client._coerce_tabular_input({"a": [1], "b": [2]}, col_names=["only_one"])


def test_pptx_mixed_per_graph_boxes_keep_lab_slide_scale(tmp_path) -> None:
    """In mixed box=[(l,t,w), (l,t)], the 2-tuple graph must still get 140% lab-slide scale."""
    import struct
    from pathlib import Path

    pptx_mod = pytest.importorskip("pptx")
    Presentation = pptx_mod.Presentation

    # Minimal valid 108-byte EMF header with rclFrame 8330 x 6250 (0.01 mm -> 8.33 x 6.25 cm)
    emf_hdr = bytearray(108)
    struct.pack_into("<II", emf_hdr, 0, 1, 108)
    struct.pack_into("<iiii", emf_hdr, 24, 0, 0, 8330, 6250)
    emf_hdr[40:44] = b" EMF"
    emf_bytes = bytes(emf_hdr)

    class _FakeSession:
        def list_windows(self):
            return [{"name": "G1", "type": "MultiLayer"}, {"name": "G2", "type": "MultiLayer"}]

        def call(self, method: str, _params=None, **params):
            if method == "graph.get_info":
                return {
                    "name": params.get("graph", "G1"),
                    "slide_paste_percent": 140,
                    "applied_style_preset": "lab_slide",
                    "layers": [{"axes": {"x": {"enabled": True, "title_font_pt": 12.9, "numbers_font_pt": 12.9}}}],
                }
            if method == "graph.export_ole":
                Path(params["emf_path"]).write_bytes(emf_bytes)
                return {
                    "prog_id": "ToriGraph.Graph.1",
                    "graph": params.get("graph", "G1"),
                    "embed_scope": "graph",
                    "width_mm": 83.3,
                    "height_mm": 62.5,
                    "sciprj_xml": "SciDAVis 3.0.0 project file\n",
                }
            if method == "graph.export":
                Path(params["path"]).write_bytes(emf_bytes)
                return {"ok": True}
            return {"ok": True}

    out_pptx = tmp_path / "mixed_box.pptx"
    res = tg_pptx.export_graphs_to_pptx(
        _FakeSession(),
        out_pptx,
        ["G1", "G2"],
        slide=1,
        box=[(1.0, 2.0, 10.0), (13.0, 2.0)],
        overwrite=True,
    )
    assert out_pptx.exists()
    prs = Presentation(str(out_pptx))
    assert len(prs.slides) == 1
    shapes = list(prs.slides[0].shapes)
    assert len(shapes) == 2
    w0_cm = shapes[0].width / tg_pptx.EMU_PER_CM
    w1_cm = shapes[1].width / tg_pptx.EMU_PER_CM
    # Graph 0 uses explicit width = 10.0 cm
    assert abs(w0_cm - 10.0) < 0.05
    # Graph 1 uses natural 8.33 cm * 1.4 = 11.662 cm (NOT unscaled 8.33 cm!)
    assert abs(w1_cm - 8.33 * tg_pptx.LAB_SLIDE_PLACEMENT_SCALE) < 0.15, (
        f"Expected ~{8.33 * tg_pptx.LAB_SLIDE_PLACEMENT_SCALE:.2f} cm, got {w1_cm:.2f} cm"
    )
    assert res["placements"][0]["effective_scale"] == pytest.approx(10.0 / 8.33, rel=1e-2)
    assert res["placements"][1]["effective_scale"] == pytest.approx(
        tg_pptx.LAB_SLIDE_PLACEMENT_SCALE, rel=1e-2
    )
    assert res["scale"] == pytest.approx(tg_pptx.LAB_SLIDE_PLACEMENT_SCALE)


def test_batch_context_explicit_execute_no_duplicate_on_exit() -> None:
    """Explicit b.execute() inside `with app.batch() as b:` must not run a second time on exit."""
    t = MockTransport(
        [
            _ok(
                {
                    "ok": True,
                    "completed": 1,
                    "total": 1,
                    "rolled_back": False,
                    "results": [{"ok": True, "name": "Table1"}],
                }
            ),
            _ok(
                {
                    "ok": True,
                    "completed": 1,
                    "total": 1,
                    "rolled_back": False,
                    "results": [{"ok": True, "name": "Table2"}],
                }
            ),
        ]
    )
    s = Session(t)
    with s.batch() as b:
        b.add("table.create", rows=5, cols=2)
        first = b.execute()
        assert first["ok"] is True
        # Second explicit execute() with no new commands is a no-op
        second = b.execute()
        assert second == first
    # Exiting the `with` block must NOT send a duplicate app.batch RPC
    assert len(t.sent) == 1
    assert t.sent[0][0]["method"] == "app.batch"
    assert len(t.sent[0][0]["params"]["commands"]) == 1

    # Adding new commands after an explicit execute() flushes ONLY the new commands on exit
    with s.batch() as b2:
        b2._flushed = True
        b2.commands.append({"method": "table.create", "params": {"rows": 1, "cols": 1}})
        b2.add("table.create", rows=3, cols=2)
    assert len(t.sent) == 2
    assert len(t.sent[1][0]["params"]["commands"]) == 1
    assert t.sent[1][0]["params"]["commands"][0]["params"]["rows"] == 3


def test_coerce_tabular_input_rejects_empty_and_duplicate_col_names() -> None:
    """Wave 3: _coerce_tabular_input must reject empty col_names on non-empty input and duplicates."""
    sample = {"a": [1.0, 2.0], "b": [3.0, 4.0]}

    with pytest.raises(ValueError, match="empty"):
        tg_client._coerce_tabular_input(sample, col_names=[])

    with pytest.raises(ValueError, match="duplicate"):
        tg_client._coerce_tabular_input(sample, col_names=["a", "a"])

    with pytest.raises(ValueError, match="duplicate"):
        tg_client._coerce_tabular_input(sample, col_names=["X", "X"])

    # Empty input with empty col_names remains valid
    cols, order, *_ = tg_client._coerce_tabular_input({}, col_names=[])
    assert cols == {} and order == []
