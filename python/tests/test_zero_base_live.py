"""Live integration tests for the zero-base UX & quality revamp (Wave A-C / Step 2)."""

from __future__ import annotations

import zipfile
from pathlib import Path

import pytest

import torigraph as tg


def test_lab_slide_style_preset(tg_app: tg.Session) -> None:
    """GraphProxy.lab_slide_style() applies lab_slide_83mm (83.3 mm / 12.9 pt)."""
    tbl = tg_app.create_table({"X": [0.0, 1.0, 2.0], "Y": [1.0, 3.0, 2.0]}, designations=["X", "Y"])
    g = tg_app.plot_xy(tbl, "X", "Y", style="line", graph_name="LiveZeroBaseLabSlide")
    try:
        res = g.lab_slide_style()
        assert isinstance(res, dict)
        info = g.info()
        assert info.get("applied_style_preset") == "lab_slide_83mm" or res.get("preset_id") == "lab_slide_83mm"
    finally:
        tg_app.close_window(g)
        tg_app.close_window(tbl)


def test_plot_box_tukey_15iqr(tg_app: tg.Session) -> None:
    """Session.plot_box(..., whisker_mode='1.5iqr') creates a Box plot with Tukey 1.5*IQR whiskers."""
    tbl = tg_app.create_table(
        {
            "A": [10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 45.0],
            "B": [20.0, 21.0, 22.0, 23.0, 24.0, 25.0, 26.0],
        }
    )
    g = tg_app.plot_box(tbl, cols=["A", "B"], whisker_mode="1.5iqr", graph_name="LiveZeroBaseBoxTukey")
    try:
        assert g.name == "LiveZeroBaseBoxTukey"
        info = g.info()
        assert int(info.get("curves", 0) or len(info.get("layers", [{}])[0].get("curves", []))) >= 2
    finally:
        tg_app.close_window(g)
        tg_app.close_window(tbl)


def test_to_python_standalone_uses_connect_or_launch(tg_app: tg.Session) -> None:
    """GraphProxy.to_python(standalone=True) emits app = tg.connect_or_launch()."""
    tbl = tg_app.create_table({"X": [1.0, 2.0, 3.0], "Y": [2.0, 4.0, 6.0]}, designations=["X", "Y"])
    g = tg_app.plot_xy(tbl, "X", "Y", style="line", graph_name="LiveZeroBaseToPy")
    try:
        script = g.to_python_script(standalone=True)
        assert "app = tg.connect_or_launch()" in script
    finally:
        tg_app.close_window(g)
        tg_app.close_window(tbl)


def test_batch_read_raises_batch_read_error(tg_app: tg.Session) -> None:
    """Synchronous reads inside with tg_app.batch(): raise tg.BatchReadError cleanly."""
    with pytest.raises(tg.BatchReadError):
        with tg_app.batch():
            tg_app.list_windows()
    # Session must remain healthy after BatchReadError unwinds the batch context.
    assert tg_app.info().get("name") == "ToriGraph"


def test_to_pptx_overwrite_protection(tg_app: tg.Session, tmp_path: Path) -> None:
    """GraphProxy.to_pptx(existing_path, overwrite=False) refuses silent overwrite; overwrite=True succeeds."""
    pytest.importorskip("pptx", reason="python-pptx is needed for to_pptx")
    tbl = tg_app.create_table({"X": [0.0, 1.0, 2.0], "Y": [0.5, 1.5, 2.5]}, designations=["X", "Y"])
    g = tg_app.plot_xy(tbl, "X", "Y", style="line", graph_name="LiveZeroBasePptx")
    try:
        g.lab_slide_style()
        out_pptx = tmp_path / "existing_deck.pptx"
        sentinel = b"DO_NOT_OVERWRITE_SENTINEL"
        out_pptx.write_bytes(sentinel)

        with pytest.raises(FileExistsError):
            g.to_pptx(out_pptx, overwrite=False)
        assert out_pptx.read_bytes() == sentinel

        res = g.to_pptx(out_pptx, overwrite=True)
        assert res and out_pptx.is_file() and out_pptx.stat().st_size > 1000
        assert zipfile.is_zipfile(out_pptx)
    finally:
        tg_app.close_window(g)
        tg_app.close_window(tbl)
