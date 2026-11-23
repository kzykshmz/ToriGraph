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
