"""SDK-only tests (no ToriGraph process): public names, deprecated aliases, typed signatures."""

from __future__ import annotations

import inspect
import json
import warnings
from pathlib import Path
from typing import Any, Dict, List, Optional

import pytest

import torigraph as tg
from torigraph.client import GraphProxy, MatrixProxy, Session, TableProxy

PKG_DIR = Path(tg.__file__).resolve().parent


class RecordingTransport:
    """Answers every request with ``{"name": ..., "ok": True}`` and records it."""

    endpoint = "fake://record"

    def __init__(self) -> None:
        self.sent: List[Dict[str, Any]] = []
        self._pending: List[bytes] = []

    def send_line(self, payload: bytes, timeout: Optional[float] = None) -> None:
        req = json.loads(payload.decode("utf-8"))
        self.sent.append(req)
        name = "Graph1" if req["method"].startswith("plot.") else "Table1"
        reply = {"jsonrpc": "2.0", "id": req["id"], "result": {"name": name, "ok": True}}
        self._pending.append(json.dumps(reply).encode("utf-8"))

    def recv_line(self, timeout: Optional[float] = None) -> bytes:
        return self._pending.pop(0)

    def close(self) -> None:
        pass

    def methods(self) -> List[str]:
        return [r["method"] for r in self.sent]


@pytest.fixture()
def rec() -> RecordingTransport:
    return RecordingTransport()


@pytest.fixture()
def app(rec: RecordingTransport) -> Session:
    return Session(rec)


def _aliases(cls: type) -> Dict[str, str]:
    out = {}
    for name, val in vars(cls).items():
        target = getattr(val, "_torigraph_alias_of", None)
        if target:
            out[name] = target
    return out


@pytest.mark.parametrize("cls", [Session, TableProxy, MatrixProxy, GraphProxy])
def test_every_alias_points_to_a_real_non_alias_method(cls: type) -> None:
    for old, canon in _aliases(cls).items():
        target = getattr(cls, canon, None)
        assert callable(target), f"{cls.__name__}.{old} -> missing {canon}"
        assert getattr(target, "_torigraph_alias_of", None) is None, f"{old} -> alias {canon}"


def test_known_aliases_are_deprecated() -> None:
    assert _aliases(TableProxy)["set_col"] == "set_column"
    assert _aliases(TableProxy)["set_column_data"] == "set_column"
    assert _aliases(TableProxy)["get_col"] == "get_column"
    assert _aliases(TableProxy)["get_info"] == "info"
    assert _aliases(GraphProxy)["fix_layout"] == "auto_layout"
    assert _aliases(Session)["kocks_mecking"] == "analyze_work_hardening"
    assert _aliases(Session)["export_python_script"] == "to_python_script"
    assert len(_aliases(Session)) >= 30


def test_deprecated_alias_warns_and_sends_the_same_rpc(app: Session, rec: RecordingTransport) -> None:
    tbl = app.table("Data1")
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        tbl.set_column("A", [1.0, 2.0])  # canonical name: no warning
    with pytest.warns(DeprecationWarning, match=r"set_col\(\).*set_column\(\)"):
        tbl.set_col("A", [1.0, 2.0])
    assert rec.methods() == ["table.set_column_data", "table.set_column_data"]
    assert rec.sent[0]["params"] == rec.sent[1]["params"]

    with pytest.warns(DeprecationWarning) as rec_w:
        app.graph("G").fix_layout()
    # the warning points at this file (the caller), not at client.py
    assert Path(rec_w[0].filename).name == Path(__file__).name
    assert rec.sent[-1]["method"] == "graph.auto_layout"


def test_batch_context_call_is_not_deprecated() -> None:
    assert getattr(tg.BatchContext.call, "_torigraph_alias_of", None) is None


def test_main_methods_have_explicit_typed_parameters() -> None:
    expected = {
        Session.fit_linear: {"graph", "curve", "table", "x_col", "y_col", "x_min", "x_max", "plot_curve"},
        Session.fit_nonlinear: {"model", "formula", "init_params", "bounds", "fixed", "max_iter"},
        Session.smooth: {"method", "window", "poly_order", "graph", "curve"},
        Session.fft: {"graph", "curve", "sampling", "inverse"},
        Session.plot_xy: {"table", "x_col", "y_cols", "x", "y", "y_err", "style", "xlabel"},
        GraphProxy.export: {"path", "dpi", "transparent", "format"},
        GraphProxy.to_pptx: {"path", "deck", "box", "preset", "scale", "embed_scope"},
        Session.to_pptx: {"path", "graphs", "preset", "scale"},
    }
    for func, names in expected.items():
        sig = inspect.signature(func)
        missing = names - set(sig.parameters)
        assert not missing, f"{func.__qualname__} lacks {missing}"
        for n in names:
            assert sig.parameters[n].annotation is not inspect.Parameter.empty, (func, n)
        assert func.__doc__ and len(func.__doc__) > 60, f"{func.__qualname__} has no docstring"


def test_fit_linear_sends_names_for_proxies(app: Session, rec: RecordingTransport) -> None:
    app.fit_linear(graph=app.graph("G1"), x_min=0.0, x_max=0.2)
    assert rec.sent[-1]["method"] == "analysis.fit_linear"
    assert rec.sent[-1]["params"] == {"graph": "G1", "x_min": 0.0, "x_max": 0.2}


def test_smooth_lambda_keyword(app: Session, rec: RecordingTransport) -> None:
    app.smooth("whittaker", graph="G1", lambda_=50.0)
    assert rec.sent[-1]["params"] == {"method": "whittaker", "graph": "G1", "lambda": 50.0}


def test_plot_xy_positional_arrays_use_second_argument_as_y(app: Session, rec: RecordingTransport) -> None:
    g = app.plot_xy([0.0, 1.0, 2.0], [0.0, 10.0, 20.0], style="line")
    assert isinstance(g, GraphProxy)
    data = rec.sent[1]["params"]["data"]  # table.create, then table.set_data_dict
    assert data == {"X": [0.0, 1.0, 2.0], "Y": [0.0, 10.0, 20.0]}
    plot = rec.sent[-1]
    assert plot["method"] == "plot.xy"
    assert plot["params"]["x_col"] == "X" and plot["params"]["y_cols"] == ["Y"]
    assert plot["params"]["style"] == "line"


def test_plot_xy_keyword_arrays_multi_y_and_errors(app: Session, rec: RecordingTransport) -> None:
    app.plot_xy(x=[1, 2, 3], y=[[1, 4, 9], [1, 8, 27]], y_err=[[0.1] * 3, [0.2] * 3], color="#0072B2")
    data = rec.sent[1]["params"]["data"]
    assert list(data) == ["X", "Y1", "Y1_err", "Y2", "Y2_err"]
    desig = rec.sent[1]["params"]["designations"]
    assert desig["Y1_err"] == "yErr" and desig["X"] == "X"
    plot = rec.sent[-1]["params"]
    assert plot["y_cols"] == ["Y1", "Y2"] and plot["y_err_cols"] == ["Y1_err", "Y2_err"]
    assert plot["color"] == "#0072B2"
    assert "y" not in plot and "x" not in plot  # no longer leaked to the RPC


def test_plot_xy_rejects_ambiguous_array_calls(app: Session) -> None:
    with pytest.raises(TypeError, match="y_cols"):
        app.plot_xy([1, 2], [3, 4], [0.1, 0.1])  # third positional is not y_err
    with pytest.raises(TypeError, match="without Y"):
        app.plot_xy([1, 2, 3])
    with pytest.raises(TypeError, match="both"):
        app.plot_xy(x=[1, 2])
    with pytest.raises(TypeError, match="not both"):
        app.plot_xy("Table1", "A", x=[1, 2], y=[3, 4])


def test_plot_xy_worksheet_mode_unchanged(app: Session, rec: RecordingTransport) -> None:
    app.plot_xy("Data1", "Strain", ["Stress"], graph_name="G")
    assert rec.methods() == ["plot.xy"]
    p = rec.sent[0]["params"]
    assert p["table"] == "Data1" and p["x_col"] == "Strain" and p["y_cols"] == ["Stress"]


def test_launch_does_not_change_language_by_default() -> None:
    assert inspect.signature(tg.launch).parameters["language"].default is None
    assert inspect.signature(tg.connect_or_launch).parameters["language"].default is None


def test_package_is_marked_typed() -> None:
    assert (PKG_DIR / "py.typed").is_file()
