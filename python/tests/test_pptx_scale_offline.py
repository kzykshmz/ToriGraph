"""Offline check of export_graphs_to_pptx(scale=..., embed_scope=...) with a fake session.

No ToriGraph process is needed (Windows + python-pptx). Run with pytest, or directly.
The whole module is skipped on non-Windows hosts (it builds an EMF through GDI) and
when python-pptx is not installed, so collection never fails on the Linux CI job.
"""
import sys
import tempfile
import warnings
from pathlib import Path

import pytest

if sys.platform != "win32":
    pytest.skip("Windows-only: builds an EMF through GDI (ctypes.windll)", allow_module_level=True)

pytest.importorskip("pptx", reason="python-pptx is not installed")

import ctypes  # noqa: E402
import ctypes.wintypes as wt  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from torigraph.pptx import export_graphs_to_pptx  # noqa: E402
from torigraph.client import RpcError  # noqa: E402


def make_emf(w_hm, h_hm):
    gdi32 = ctypes.windll.gdi32
    gdi32.CreateEnhMetaFileW.restype = wt.HANDLE
    gdi32.CloseEnhMetaFile.restype = wt.HANDLE
    gdi32.CloseEnhMetaFile.argtypes = [wt.HANDLE]
    gdi32.GetEnhMetaFileBits.argtypes = [wt.HANDLE, wt.UINT, ctypes.c_void_p]
    gdi32.DeleteEnhMetaFile.argtypes = [wt.HANDLE]
    gdi32.Rectangle.argtypes = [wt.HANDLE, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    gdi32.CreateEnhMetaFileW.argtypes = [wt.HANDLE, wt.LPCWSTR, ctypes.POINTER(wt.RECT), wt.LPCWSTR]
    rc = wt.RECT(0, 0, w_hm, h_hm)
    dc = gdi32.CreateEnhMetaFileW(None, None, ctypes.byref(rc), None)
    gdi32.Rectangle(dc, 10, 10, 200, 200)
    h = gdi32.CloseEnhMetaFile(dc)
    n = gdi32.GetEnhMetaFileBits(h, 0, None)
    buf = ctypes.create_string_buffer(n)
    gdi32.GetEnhMetaFileBits(h, n, buf)
    gdi32.DeleteEnhMetaFile(h)
    return buf.raw


@pytest.fixture(scope="module")
def emf_bytes():
    """83.33 mm square page (slide preset at 100 %)."""
    return make_emf(8333, 8333)


class FakeSession:
    def __init__(self, emf, fail_export_ole=False, scope_reply="graph"):
        self.emf = emf
        self.fail = fail_export_ole
        self.scope_reply = scope_reply
        self.calls = []

    def list_windows(self):
        return [{"name": "Graph1", "type": "MultiLayer"}, {"name": "Graph2", "type": "MultiLayer"}]

    def save_project(self, path, compress=False, snapshot=False):
        self.calls.append(("save_project", {"path": str(path), "compress": compress, "snapshot": snapshot}))
        Path(path).write_text("whole project", encoding="utf-8")

    def call(self, method, _params=None, **kw):
        self.calls.append((method, kw))
        if method == "graph.export_ole":
            if self.fail:
                raise RpcError(-1, "boom")
            Path(kw["emf_path"]).write_bytes(self.emf)
            res = {"prog_id": "ToriGraph.Graph.1", "graph": kw["graph"], "width_mm": 83.33,
                   "height_mm": 83.33, "sciprj_xml": "proj of " + kw["graph"]}
            if self.scope_reply:
                res["embed_scope"] = self.scope_reply
            return res
        if method == "graph.export":
            Path(kw["path"]).write_bytes(self.emf)
            return {"ok": True}
        if method == "graph.get_info":
            ax = {"enabled": True, "title_font_pt": 20, "numbers_font_pt": 20}
            return {"layers": [{"axes": {"x": ax, "y": ax, "top": {"enabled": False},
                                         "right": {"enabled": False}}}]}
        raise AssertionError(method)


def check(cond, msg):
    assert cond, msg


def test_export_graphs_to_pptx_scale_effective_pt_and_warnings(emf_bytes):
    tmp = Path(tempfile.mkdtemp())
    # 1. scale=1.4: 8.333 cm -> 11.67 cm, 20 pt -> 28 pt
    s = FakeSession(emf_bytes)
    with warnings.catch_warnings(record=True) as w:
        warnings.simplefilter("always")
        res = export_graphs_to_pptx(s, tmp / "a.pptx", ["Graph1"], scale="140%")
    pl = res["placements"][0]
    check(abs(pl["effective_scale"] - 1.4) < 1e-3, f"effective_scale 1.4 ({pl['effective_scale']})")
    check(abs(pl["width_cm"] - 11.666) < 0.01, f"width 11.67 cm ({pl['width_cm']})")
    check(pl["effective_axis_title_pt"] == 28.0 and pl["effective_tick_label_pt"] == 28.0,
          f"effective pt 28 ({pl['effective_axis_title_pt']})")
    check(res["warnings"] == [] and not w, f"no warnings ({res['warnings']})")
    oc = [c for c in s.calls if c[0] == "graph.export_ole"][0][1]
    check(oc.get("embed_scope") == "graph" and oc.get("skip_project_stream") is False,
          "embed_scope=graph requested, project stream not skipped")

    # 2. two graphs, graph scope: each export_ole asks for its own project (no skip)
    s = FakeSession(emf_bytes)
    res = export_graphs_to_pptx(s, tmp / "b.pptx", ["Graph1", "Graph2"], per_slide=2, scale=1.0)
    skips = [c[1]["skip_project_stream"] for c in s.calls if c[0] == "graph.export_ole"]
    check(skips == [False, False], f"no project-stream reuse across graphs ({skips})")
    check(all(abs(p["effective_scale"] - 1.0) < 1e-3 for p in res["placements"]), "scale 1.0 kept")

    # 3. project scope reuses the cache like before
    s = FakeSession(emf_bytes, scope_reply="project")
    res = export_graphs_to_pptx(s, tmp / "c.pptx", ["Graph1", "Graph2"], embed_scope="project")
    skips = [c[1]["skip_project_stream"] for c in s.calls if c[0] == "graph.export_ole"]
    check(skips == [False, True], f"project scope reuses cache ({skips})")
    check("effective_scale" in res["placements"][0], "effective scale reported without scale=")

    # 4. export_ole failure: warn instead of silently embedding the whole project
    s = FakeSession(emf_bytes, fail_export_ole=True)
    with warnings.catch_warnings(record=True) as w:
        warnings.simplefilter("always")
        res = export_graphs_to_pptx(s, tmp / "d.pptx", ["Graph1"])
    check(any("graph.export_ole failed" in m for m in res["warnings"]), "export_ole failure warned")
    check(any("whole project" in m for m in res["warnings"]), "whole-project fallback warned")
    check(len(w) >= 2, f"UserWarnings issued ({len(w)})")
    sp_calls = [c[1] for c in s.calls if c[0] == "save_project"]
    check(len(sp_calls) == 1 and sp_calls[0]["snapshot"] is True and sp_calls[0]["compress"] is False,
          f"save_project must be called with snapshot=True ({sp_calls})")

    # 4b. legacy FakeSession without snapshot= parameter falls back cleanly via TypeError
    class LegacyFakeSession(FakeSession):
        def save_project(self, path, compress=False):
            self.calls.append(("legacy_save_project", {"path": str(path), "compress": compress}))
            Path(path).write_text("legacy whole project", encoding="utf-8")

    s_leg = LegacyFakeSession(emf_bytes, fail_export_ole=True)
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        export_graphs_to_pptx(s_leg, tmp / "d_legacy.pptx", ["Graph1"])
    leg_calls = [c[1] for c in s_leg.calls if c[0] == "legacy_save_project"]
    check(len(leg_calls) == 1 and leg_calls[0]["compress"] is False,
          f"legacy save_project fallback failed ({leg_calls})")

    # 5. box with width + scale is rejected; box=(left, top) + scale works
    try:
        export_graphs_to_pptx(FakeSession(emf_bytes), tmp / "e.pptx", ["Graph1"], box=(1, 1, 10), scale=1.4)
        check(False, "box width + scale rejected")
    except ValueError:
        check(True, "box width + scale rejected")
    res = export_graphs_to_pptx(FakeSession(emf_bytes), tmp / "f.pptx", ["Graph1"], box=(1.0, 2.0), scale=1.4)
    pl = res["placements"][0]
    check(abs(pl["left_cm"] - 1.0) < 1e-3 and abs(pl["top_cm"] - 2.0) < 1e-3
          and abs(pl["width_cm"] - 11.666) < 0.01, "box=(left, top) + scale")

    # 6. oversize warns
    res = export_graphs_to_pptx(FakeSession(emf_bytes), tmp / "g.pptx", ["Graph1"], scale=3.0)
    check(any("beyond slide" in m for m in res["warnings"]), "oversize placement warned")


if __name__ == "__main__":
    test_export_graphs_to_pptx_scale_effective_pt_and_warnings(make_emf(8333, 8333))
    print("ALL OK")
