"""SDK-only tests (no ToriGraph process): JSON-RPC id matching, timeouts, versions."""

from __future__ import annotations

import json
import re
import subprocess
import sys
import warnings
from collections import deque
from pathlib import Path
from typing import Any, Deque, List, Optional, Tuple

import pytest

import torigraph as tg
from torigraph.client import Session, check_app_version

REPO_ROOT = Path(__file__).resolve().parents[2]


class FakeTransport:
    """Scripted transport: each recv_line pops the next queued reply (or times out)."""

    endpoint = "fake://test"

    def __init__(self, replies: List[Any]) -> None:
        self.replies: Deque[Any] = deque(replies)
        self.sent: List[Tuple[dict, Optional[float]]] = []
        self.recv_timeouts: List[Optional[float]] = []
        self.closed = False

    def send_line(self, payload: bytes, timeout: Optional[float] = None) -> None:
        if self.closed:
            raise tg.TransportError("closed")
        self.sent.append((json.loads(payload.decode("utf-8")), timeout))

    def recv_line(self, timeout: Optional[float] = None) -> bytes:
        self.recv_timeouts.append(timeout)
        if self.closed:
            raise tg.TransportError("closed")
        if not self.replies:
            self.closed = True
            raise tg.TransportError(f"Timed out after {timeout:.1f}s waiting for a response")
        item = self.replies.popleft()
        if callable(item):
            item = item(self.sent[-1][0])
        return json.dumps(item).encode("utf-8") if not isinstance(item, bytes) else item

    def close(self) -> None:
        self.closed = True


def _ok(result: Any) -> Any:
    return lambda req: {"jsonrpc": "2.0", "id": req["id"], "result": result}


def test_matching_id_returns_result() -> None:
    t = FakeTransport([_ok({"pong": True})])
    s = Session(t)
    assert s.call("ping") == {"pong": True}
    assert t.sent[0][0]["id"] == 1


def test_stale_response_is_discarded() -> None:
    # A late answer to request 3 arrives while request 5 is waiting: it must be skipped.
    t = FakeTransport([{"jsonrpc": "2.0", "id": 3, "result": {"stale": True}}, _ok({"fresh": True})])
    s = Session(t)
    s._next_id = 5
    assert s.call("app.info") == {"fresh": True}
    assert s.discarded_responses == 1
    assert s.usable


def test_unexpected_future_id_invalidates_session() -> None:
    t = FakeTransport([{"jsonrpc": "2.0", "id": 99, "result": {}}])
    s = Session(t)
    with pytest.raises(tg.TransportError, match="id 99"):
        s.call("ping")
    assert not s.usable
    with pytest.raises(tg.TransportError, match="no longer usable"):
        s.call("ping")


def test_timeout_invalidates_session_instead_of_shifting_answers() -> None:
    # Old behaviour: the late answer to call 1 became the result of call 2.
    t = FakeTransport([])
    s = Session(t)
    with pytest.raises(tg.TransportError, match="Timed out"):
        s.call("analysis.slow", rpc_timeout=0.5)
    t.replies.append({"jsonrpc": "2.0", "id": 1, "result": {"late": True}})
    with pytest.raises(tg.TransportError, match="no longer usable"):
        s.call("ping")


def test_named_pipe_timeout_gets_rpc_timeout_hint() -> None:
    # The named-pipe transport words it "Named pipe read timed out after ..." (lower case).
    class PipeLikeTransport(FakeTransport):
        def recv_line(self, timeout: Optional[float] = None) -> bytes:
            raise tg.TransportError(f"Named pipe read timed out after {timeout:.2f}s (fake).")

    s = Session(PipeLikeTransport([]))
    with pytest.raises(tg.TransportError, match="rpc_timeout"):
        s.call("analysis.slow", rpc_timeout=0.5)
    assert not s.usable


def test_rpc_timeout_per_call_and_per_session() -> None:
    t = FakeTransport([_ok({}), _ok({})])
    s = Session(t, rpc_timeout=7.0)
    s.call("ping")
    s.call("table.info", rpc_timeout=600)
    assert t.sent[0][1] == pytest.approx(7.0)
    assert t.sent[1][1] == pytest.approx(600.0)
    # rpc_timeout is an SDK option, never an RPC parameter
    assert "rpc_timeout" not in t.sent[1][0]["params"]
    assert tg.DEFAULT_RPC_TIMEOUT >= 60


def test_null_id_error_is_reported_as_rpc_error() -> None:
    t = FakeTransport([{"jsonrpc": "2.0", "id": None, "error": {"code": -32700, "message": "Parse error"}}])
    s = Session(t)
    with pytest.raises(tg.RpcError) as ei:
        s.call("ping")
    assert ei.value.code == -32700
    assert s.usable


def test_rpc_error_keeps_session_usable() -> None:
    t = FakeTransport(
        [
            lambda req: {"jsonrpc": "2.0", "id": req["id"], "error": {"code": -32601, "message": "nope"}},
            _ok({"pong": True}),
        ]
    )
    s = Session(t)
    with pytest.raises(tg.RpcError):
        s.call("no.such.method")
    assert s.call("ping") == {"pong": True}


def test_version_check_warns_on_mismatch() -> None:
    s = Session(FakeTransport([_ok({"version": "0.0.1"})]))
    with pytest.warns(tg.ToriGraphVersionWarning, match="0.0.1"):
        res = check_app_version(s)
    assert res["match"] is False and res["sdk_version"] == tg.__version__


def test_version_check_silent_on_match() -> None:
    s = Session(FakeTransport([_ok({"version": tg.__version__})]))
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        assert check_app_version(s)["match"] is True


def test_sdk_version_matches_cmake_project_version() -> None:
    text = (REPO_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    m = re.search(r"project\s*\(\s*scidavis\b[^)]*?\bVERSION\s+([0-9.]+)", text, re.S)
    assert m, "project(... VERSION ...) not found"
    assert tg.__version__ == m.group(1)


def test_check_version_script() -> None:
    script = REPO_ROOT / "scripts" / "check_version.py"
    ok = subprocess.run([sys.executable, str(script)], capture_output=True, text=True)
    assert ok.returncode == 0, ok.stderr
    good = subprocess.run(
        [sys.executable, str(script), "--tag", f"v{tg.__version__}", "--print"],
        capture_output=True,
        text=True,
    )
    assert good.returncode == 0 and good.stdout.strip() == tg.__version__
    bad = subprocess.run([sys.executable, str(script), "--tag", "v0.0.0"], capture_output=True, text=True)
    assert bad.returncode == 1 and "does not match" in bad.stderr


def test_live_app_version_matches_sdk(tg_app) -> None:
    info = tg_app.info()
    assert info.get("version") == tg.__version__
    assert check_app_version(tg_app, warn=False)["match"] is True
