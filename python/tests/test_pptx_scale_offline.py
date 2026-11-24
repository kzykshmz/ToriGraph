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
from torigraph.client import RpcError, ToriGraphError  # noqa: E402


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

    # 4. export_ole failure (or missing embed_scope): refused by default (allow_whole_project=False);
    #    explicit allow_whole_project=True falls back with warnings.
    s = FakeSession(emf_bytes, fail_export_ole=True)
    with pytest.raises(ToriGraphError, match="allow_whole_project=True"):
        export_graphs_to_pptx(s, tmp / "d_blocked.pptx", ["Graph1"])
    assert not (tmp / "d_blocked.pptx").exists()

    s_noscope = FakeSession(emf_bytes, scope_reply=None)
    with pytest.raises(ToriGraphError, match="allow_whole_project=True"):
        export_graphs_to_pptx(s_noscope, tmp / "d_noscope.pptx", ["Graph1"])

    s = FakeSession(emf_bytes, fail_export_ole=True)
    with warnings.catch_warnings(record=True) as w:
        warnings.simplefilter("always")
        res = export_graphs_to_pptx(s, tmp / "d.pptx", ["Graph1"], allow_whole_project=True)
    check(any("graph.export_ole failed" in m for m in res["warnings"]), "export_ole failure warned")
    check(any("whole project" in m for m in res["warnings"]), "whole-project fallback warned")
    check(len(w) >= 2, f"UserWarnings issued ({len(w)})")
    sp_calls = [c[1] for c in s.calls if c[0] == "save_project"]
    check(len(sp_calls) == 1 and sp_calls[0]["snapshot"] is True and sp_calls[0]["compress"] is False,
          f"save_project must be called with snapshot=True ({sp_calls})")

    # 4b. legacy FakeSession without snapshot= parameter must NOT silently mutate project path via non-snapshot save_project
    class LegacyFakeSession(FakeSession):
        def save_project(self, path, compress=False):
            self.calls.append(("legacy_save_project", {"path": str(path), "compress": compress}))
            Path(path).write_text("legacy whole project", encoding="utf-8")

    s_leg = LegacyFakeSession(emf_bytes, fail_export_ole=True)
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        with pytest.raises((TypeError, ToriGraphError)):
            export_graphs_to_pptx(s_leg, tmp / "d_legacy.pptx", ["Graph1"], allow_whole_project=True)
    leg_calls = [c[1] for c in s_leg.calls if c[0] == "legacy_save_project"]
    check(len(leg_calls) == 0, f"legacy save_project without snapshot= must not be called ({leg_calls})")

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


def test_pptx_sanitizes_forbidden_temp_chars(emf_bytes, tmp_path):
    """D3-3: graph names with / : * ? < > | or reserved device names work on Windows."""
    s = FakeSession(emf_bytes)
    out = tmp_path / "forbidden_names.pptx"
    res = export_graphs_to_pptx(s, out, ["Al/Zn:Sample*1?", "CON"])
    assert res["ok"] is True
    assert res["embedded_count"] == 2
    assert out.is_file() and out.stat().st_size > 1000


def test_pptx_no_silent_overwrite_and_atomic_replace(emf_bytes, tmp_path, monkeypatch):
    """C5-#14: existing out_path requires overwrite=True and saves atomically via os.replace."""
    import os
    from pptx import Presentation

    s = FakeSession(emf_bytes)
    out = tmp_path / "deck.pptx"
    export_graphs_to_pptx(s, out, ["Graph1"])
    sentinel_bytes = b"not-overwritten"
    out.write_bytes(sentinel_bytes)

    with pytest.raises(FileExistsError, match="overwrite=True"):
        export_graphs_to_pptx(s, out, ["Graph1"])
    assert out.read_bytes() == sentinel_bytes

    # Even with replace=True or slide=1 when deck is None, existing file must not be touched
    prs_valid = Presentation()
    prs_valid.slides.add_slide(prs_valid.slide_layouts[6])
    prs_valid.save(str(out))
    before = out.read_bytes()
    with pytest.raises(FileExistsError, match="overwrite=True"):
        export_graphs_to_pptx(s, out, ["Graph1"], replace=True)
    assert out.read_bytes() == before

    replace_calls = []
    real_replace = os.replace

    def spy_replace(src, dst):
        replace_calls.append((Path(src), Path(dst)))
        return real_replace(src, dst)

    monkeypatch.setattr(os, "replace", spy_replace)
    res = export_graphs_to_pptx(s, out, ["Graph1"], slide=1, overwrite=True)
    assert res["ok"] is True
    assert len(replace_calls) == 1
    assert replace_calls[0][1] == out
    assert not replace_calls[0][0].exists()


def test_pptx_lab_slide_auto_140_percent(emf_bytes, tmp_path):
    """D3-4: 140% placement triggers for preset='lab_slide_83mm' AND previously applied graphs."""
    # 1. Previously applied via graph.get_info (applied_style_preset / slide_paste_percent)
    class LabInfoSession(FakeSession):
        def call(self, method, _params=None, **kw):
            if method == "graph.get_info":
                self.calls.append((method, kw))
                ax = {"enabled": True, "title_font_pt": 12.9, "numbers_font_pt": 12.9}
                return {
                    "applied_style_preset": "lab_slide_83mm",
                    "slide_paste_percent": 140.0,
                    "layers": [{"axes": {"x": ax, "y": ax}}],
                }
            return super().call(method, _params, **kw)

    res = export_graphs_to_pptx(LabInfoSession(emf_bytes), tmp_path / "lab_info.pptx", ["Graph1"])
    pl = res["placements"][0]
    assert abs(pl["effective_scale"] - 1.4) < 1e-3
    assert abs(pl["effective_axis_title_pt"] - 18.06) < 0.05

    # 2. Tracked on Session._slide_preset_graphs when server info lacks preset metadata
    s_tracked = FakeSession(emf_bytes)
    s_tracked._slide_preset_graphs = {"Graph1"}
    res2 = export_graphs_to_pptx(s_tracked, tmp_path / "lab_tracked.pptx", ["Graph1"])
    assert abs(res2["placements"][0]["effective_scale"] - 1.4) < 1e-3

    # 3. Explicit box width overrides automatic 140%
    res3 = export_graphs_to_pptx(
        LabInfoSession(emf_bytes), tmp_path / "lab_box.pptx", ["Graph1"], box=(1.0, 1.0, 10.0)
    )
    assert abs(res3["placements"][0]["width_cm"] - 10.0) < 0.02

    # 4. Alias preset="lab_slide" also triggers 140%
    class PresetSession(FakeSession):
        def call(self, method, _params=None, **kw):
            if method == "graph.apply_style_preset":
                self.calls.append((method, kw))
                return {"ok": True}
            return super().call(method, _params, **kw)

    res4 = export_graphs_to_pptx(
        PresetSession(emf_bytes), tmp_path / "lab_alias.pptx", ["Graph1"], preset="lab_slide"
    )
    assert abs(res4["placements"][0]["effective_scale"] - 1.4) < 1e-3


def test_pptx_skips_graph3d_and_reraises_2d_rpc_error(emf_bytes, tmp_path):
    """E2-10: preset skips Graph3D with warning and never swallows RpcError on 2D graphs."""
    class MixedSession(FakeSession):
        def __init__(self, emf, fail_2d=False):
            super().__init__(emf)
            self.fail_2d = fail_2d

        def list_windows(self):
            return [
                {"name": "Graph1", "type": "MultiLayer"},
                {"name": "Graph3D1", "type": "Graph3D"},
            ]

        def call(self, method, _params=None, **kw):
            if method == "graph.apply_style_preset":
                self.calls.append((method, kw))
                if self.fail_2d:
                    raise RpcError(-32602, f"Graph '{kw['graph']}' not found")
                return {"ok": True}
            return super().call(method, _params, **kw)

    s_ok = MixedSession(emf_bytes, fail_2d=False)
    res = export_graphs_to_pptx(s_ok, tmp_path / "mixed.pptx", preset="lab_slide_83mm")
    preset_targets = [c[1]["graph"] for c in s_ok.calls if c[0] == "graph.apply_style_preset"]
    assert preset_targets == ["Graph1"]
    assert res["embedded_count"] == 2
    assert any("Graph3D1" in w for w in res["warnings"])

    s_err = MixedSession(emf_bytes, fail_2d=True)
    with pytest.raises(RpcError, match="Graph1"):
        export_graphs_to_pptx(s_err, tmp_path / "err.pptx", ["Graph1"], preset="lab_slide_83mm")


def test_pptx_strict_ooxml_ole_limits_and_locked_permission(emf_bytes, tmp_path, monkeypatch):
    """Verify Strict OOXML rejection, >128 MB stream guard, and friendly locked-file error."""
    import os
    import zipfile
    from torigraph.pptx import _check_pptx_writable, _validate_ole_payload_sizes

    strict_pptx = tmp_path / "strict_deck.pptx"
    with zipfile.ZipFile(strict_pptx, "w") as zf:
        zf.writestr(
            "[Content_Types].xml",
            '<Types xmlns="http://purl.oclc.org/ooxml/spreadsheetml/main"></Types>',
        )
    with pytest.raises(ToriGraphError, match="Strict OOXML"):
        export_graphs_to_pptx(
            FakeSession(emf_bytes), tmp_path / "out_strict.pptx", ["Graph1"], deck=strict_pptx
        )

    strict_pres_pptx = tmp_path / "strict_presentation_deck.pptx"
    with zipfile.ZipFile(strict_pres_pptx, "w") as zf:
        zf.writestr(
            "[Content_Types].xml",
            '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"></Types>',
        )
        zf.writestr(
            "ppt/presentation.xml",
            '<p:presentation xmlns:p="http://purl.oclc.org/ooxml/presentationml/main" conformance="strict"/>',
        )
    with pytest.raises(ToriGraphError, match="Strict OOXML"):
        export_graphs_to_pptx(
            FakeSession(emf_bytes),
            tmp_path / "out_strict_pres.pptx",
            ["Graph1"],
            deck=strict_pres_pptx,
        )

    oversize_buf = b"x" * 128_000_001
    with pytest.raises(ValueError, match="128 MB"):
        _validate_ole_payload_sizes([("ToriGraphProject", oversize_buf)])

    locked = tmp_path / "open_in_powerpoint.pptx"
    locked.write_bytes(b"placeholder")
    real_open = open

    def deny_write(file, mode="r", *args, **kwargs):
        if Path(file) == locked and "r+b" in str(mode):
            raise PermissionError(13, "Sharing violation", str(file))
        return real_open(file, mode, *args, **kwargs)

    monkeypatch.setattr("builtins.open", deny_write)
    with pytest.raises(PermissionError, match="Close the presentation in PowerPoint"):
        _check_pptx_writable(locked)

    # Atomic save: os.replace raising PermissionError must preserve existing deck and clean .tmp
    target_deck = tmp_path / "locked_replace.pptx"
    sentinel = b"existing-valid-deck-bytes"
    target_deck.write_bytes(sentinel)

    def fail_replace(src, dst):
        raise PermissionError(13, "Sharing violation on replace", str(dst))

    monkeypatch.setattr("builtins.open", real_open)
    monkeypatch.setattr(os, "replace", fail_replace)
    with pytest.raises(PermissionError, match="Close the presentation in PowerPoint"):
        export_graphs_to_pptx(
            FakeSession(emf_bytes), target_deck, ["Graph1"], overwrite=True
        )
    assert target_deck.read_bytes() == sentinel
    assert not [p for p in tmp_path.iterdir() if p.suffix == ".tmp"]


def test_pptx_project_scope_downgrade_preserves_graph2_payload(emf_bytes, tmp_path):
    """Downgrading Graph1 in embed_scope='project' must NOT cache Graph1 XML onto Graph2."""
    import zipfile

    class DowngradeSession(FakeSession):
        def call(self, method, _params=None, **kw):
            self.calls.append((method, kw))
            if method == "graph.export_ole":
                Path(kw["emf_path"]).write_bytes(self.emf)
                g_name = kw["graph"]
                if g_name == "Graph1":
                    return {
                        "prog_id": "ToriGraph.Graph.1",
                        "graph": "Graph1",
                        "width_mm": 83.33,
                        "height_mm": 83.33,
                        "embed_scope": "graph",
                        "scope_downgraded": True,
                        "sciprj_xml": "<sciprj>UNIQUE_GRAPH1_XML</sciprj>",
                    }
                assert kw["embed_scope"] == "graph", f"Graph2 received embed_scope={kw['embed_scope']!r}"
                assert kw["skip_project_stream"] is False, "Graph2 must not skip its own project stream"
                return {
                    "prog_id": "ToriGraph.Graph.1",
                    "graph": "Graph2",
                    "width_mm": 83.33,
                    "height_mm": 83.33,
                    "embed_scope": "graph",
                    "sciprj_xml": "<sciprj>UNIQUE_GRAPH2_XML</sciprj>",
                }
            if method == "graph.get_info":
                ax = {"enabled": True, "title_font_pt": 20, "numbers_font_pt": 20}
                return {"layers": [{"axes": {"x": ax, "y": ax}}]}
            raise AssertionError(method)

    s = DowngradeSession(emf_bytes)
    out = tmp_path / "downgraded_batch.pptx"
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always")
        res = export_graphs_to_pptx(s, out, ["Graph1", "Graph2"], embed_scope="project", per_slide=2)

    assert res["ok"] is True
    assert res["embed_scope"] == "graph"
    assert res["embedded_count"] == 2
    assert res["static_fallback_count"] == 0
    assert any("downgraded" in w.lower() for w in res["warnings"])
    assert any("downgraded" in str(w.message).lower() for w in caught)

    ole_calls = [c[1] for c in s.calls if c[0] == "graph.export_ole"]
    assert [(c["graph"], c["embed_scope"], c["skip_project_stream"]) for c in ole_calls] == [
        ("Graph1", "project", False),
        ("Graph2", "graph", False),
    ]

    with zipfile.ZipFile(out, "r") as zf:
        ole1 = zf.read("ppt/embeddings/oleObject1.bin")
        ole2 = zf.read("ppt/embeddings/oleObject2.bin")
    assert b"UNIQUE_GRAPH1_XML" in ole1 and b"UNIQUE_GRAPH2_XML" not in ole1
    assert b"UNIQUE_GRAPH2_XML" in ole2 and b"UNIQUE_GRAPH1_XML" not in ole2


def test_pptx_oversize_ole_falls_back_to_static_emf(emf_bytes, tmp_path):
    """Both RPC ole_exceeds_limits and local >128MB/>=256MB checks fall back cleanly to static EMF."""
    import zipfile

    class LimitSession(FakeSession):
        def __init__(self, emf, mode):
            super().__init__(emf)
            self.mode = mode

        def call(self, method, _params=None, **kw):
            self.calls.append((method, kw))
            if method == "graph.export_ole":
                Path(kw["emf_path"]).write_bytes(self.emf)
                g_name = kw["graph"]
                if g_name == "Graph1" and self.mode == "rpc_exceeds":
                    return {
                        "ok": True,
                        "prog_id": "ToriGraph.Graph.1",
                        "graph": "Graph1",
                        "width_mm": 83.33,
                        "height_mm": 83.33,
                        "embed_scope": "graph",
                        "ole_exceeds_limits": True,
                        "limit_reason": "Embedded OLE project stream (130000000 bytes) exceeds the 128 MB limit.",
                    }
                if g_name == "Graph1" and self.mode == "local_cfb_256mb":
                    if kw["embed_scope"] == "project":
                        return {
                            "ok": True,
                            "prog_id": "ToriGraph.Graph.1",
                            "graph": "Graph1",
                            "width_mm": 83.33,
                            "height_mm": 83.33,
                            "embed_scope": "project",
                            "sciprj_xml": "P" * 127_500_000,
                        }
                    return {
                        "ok": True,
                        "prog_id": "ToriGraph.Graph.1",
                        "graph": "Graph1",
                        "width_mm": 83.33,
                        "height_mm": 83.33,
                        "embed_scope": "graph",
                        "sciprj_xml": "G" * 127_500_000,
                    }
                return {
                    "ok": True,
                    "prog_id": "ToriGraph.Graph.1",
                    "graph": g_name,
                    "width_mm": 83.33,
                    "height_mm": 83.33,
                    "embed_scope": "graph",
                    "sciprj_xml": "<sciprj>SMALL_GRAPH2</sciprj>",
                }
            if method == "graph.get_info":
                ax = {"enabled": True, "title_font_pt": 20, "numbers_font_pt": 20}
                return {"layers": [{"axes": {"x": ax, "y": ax}}]}
            raise AssertionError(method)

    # Case A: Server flags ole_exceeds_limits=True and omits sciprj_xml (allow_whole_project=False)
    s_rpc = LimitSession(emf_bytes, "rpc_exceeds")
    out_rpc = tmp_path / "rpc_limit_fallback.pptx"
    with warnings.catch_warnings(record=True) as w_rpc:
        warnings.simplefilter("always")
        res_rpc = export_graphs_to_pptx(s_rpc, out_rpc, ["Graph1", "Graph2"], per_slide=2)
    assert res_rpc["ok"] is True
    assert res_rpc["embedded_count"] == 2
    assert res_rpc["static_fallback_count"] == 1
    assert [p["is_ole"] for p in res_rpc["placements"]] == [False, True]
    assert any("static EMF" in msg for msg in res_rpc["warnings"])
    assert any("static EMF" in str(item.message) for item in w_rpc)
    assert not [c for c in s_rpc.calls if c[0] == "save_project"]
    with zipfile.ZipFile(out_rpc, "r") as zf:
        ole_parts = [n for n in zf.namelist() if n.startswith("ppt/embeddings/")]
        emf_parts = [n for n in zf.namelist() if n.startswith("ppt/media/")]
        assert len(ole_parts) == 1 and len(emf_parts) == 2

    # Case B: Client-side 256 MB CFB (127.5 MB x 2 + overhead) downgrades project->graph, then falls back to static EMF
    s_cfb = LimitSession(emf_bytes, "local_cfb_256mb")
    out_cfb = tmp_path / "cfb_limit_fallback.pptx"
    with warnings.catch_warnings(record=True) as w_cfb:
        warnings.simplefilter("always")
        res_cfb = export_graphs_to_pptx(
            s_cfb, out_cfb, ["Graph1", "Graph2"], embed_scope="project", per_slide=2
        )
    assert res_cfb["ok"] is True
    assert res_cfb["embed_scope"] == "graph"
    assert res_cfb["static_fallback_count"] == 1
    assert [p["is_ole"] for p in res_cfb["placements"]] == [False, True]
    assert any("downgraded to per-graph scope" in msg for msg in res_cfb["warnings"])
    assert any("static EMF" in msg for msg in res_cfb["warnings"])
    assert len(w_cfb) >= 2


if __name__ == "__main__":
    test_export_graphs_to_pptx_scale_effective_pt_and_warnings(make_emf(8333, 8333))
    print("ALL OK")
