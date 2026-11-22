"""Quick headless smoke test run by CI and before every release.

Loads the tensile sample, runs the 0.2 % proof-stress analysis and exports PNG and
PPTX -- the path most lab users take first.
"""

from __future__ import annotations

from pathlib import Path

import pytest


def test_smoke_tensile_export(tg_app, tmp_path: Path) -> None:
    info = tg_app.info()
    assert info.get("name") == "ToriGraph"
    sample = tg_app.load_sample_dataset("tensile_alznmg", auto_plot=True)
    g = tg_app.graph(sample["graph"])
    res = g.analyze_tensile(offset=0.002, plot_construction=True)
    assert res, "analyze_tensile returned nothing"
    out_png = tmp_path / "ci_smoke.png"
    g.export(out_png, dpi=300)
    assert out_png.is_file() and out_png.stat().st_size > 1000


def test_smoke_pptx(tg_app, tmp_path: Path) -> None:
    pytest.importorskip("pptx", reason="python-pptx is needed for to_pptx")
    sample = tg_app.load_sample_dataset("tensile_alznmg", auto_plot=True)
    g = tg_app.graph(sample["graph"])
    out_pptx = tmp_path / "ci_smoke.pptx"
    g.to_pptx(out_pptx, preset="slide_28pt")
    assert out_pptx.is_file() and out_pptx.stat().st_size > 1000
