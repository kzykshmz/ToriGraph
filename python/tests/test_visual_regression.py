"""Visual regression tests for ToriGraph rendering & export pipelines.

Verifies:
1. Offline image comparison & diff heatmap generation (`visual_regression.py`)
2. Live 150 DPI vs 600 DPI scale invariance (prevents high-DPI legend/tick/symbol drift)
3. Live PNG raster export vs vector PDF export perceptual parity (via PyMuPDF rasterization)
"""
from __future__ import annotations

import math
from pathlib import Path

import pytest

PIL = pytest.importorskip("PIL")
from PIL import Image, ImageDraw  # noqa: E402

from visual_regression import (  # noqa: E402
    assert_matches_golden,
    compare_images,
    load_rgb_image,
)


@pytest.mark.visual
def test_visual_diff_engine_and_golden_workflow(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    ref = Image.new("RGB", (200, 200), (255, 255, 255))
    draw_ref = ImageDraw.Draw(ref)
    draw_ref.rectangle([20, 20, 180, 180], outline=(0, 0, 0), width=2)
    draw_ref.line([25, 170, 175, 30], fill=(0, 114, 178), width=3)
    act_ok_path = tmp_path / "actual_ok.png"
    golden_path = tmp_path / "golden.png"
    ref.save(act_ok_path)

    # First run with TORIGRAPH_UPDATE_GOLDENS=1 creates the baseline
    monkeypatch.setenv("TORIGRAPH_UPDATE_GOLDENS", "1")
    created, m0 = assert_matches_golden(act_ok_path, golden_path)
    assert created and m0 is None and golden_path.is_file()

    # Subsequent run verifies exact match
    monkeypatch.delenv("TORIGRAPH_UPDATE_GOLDENS", raising=False)
    ok, metrics = assert_matches_golden(act_ok_path, golden_path, diff_dir=tmp_path)
    assert ok and metrics is not None
    assert metrics.rmse == 0.0
    assert metrics.differing_pixel_ratio == 0.0
    assert metrics.ink_ratio_actual > 0.02

    # Perturb image and verify regression detection + diff heatmap output
    bad = ref.copy()
    draw_bad = ImageDraw.Draw(bad)
    draw_bad.rectangle([60, 60, 120, 120], fill=(220, 20, 20))
    act_bad_path = tmp_path / "actual_bad.png"
    bad.save(act_bad_path)

    with pytest.raises(AssertionError, match="Visual regression"):
        assert_matches_golden(act_bad_path, golden_path, diff_dir=tmp_path, max_rmse=2.0)

    heatmap = tmp_path / "golden_diff.png"
    assert heatmap.is_file()


@pytest.mark.visual
def test_live_dpi_scale_invariance_and_pdf_vector_parity(tg_app, tmp_path: Path) -> None:
    try:
        import pymupdf  # noqa: F401
    except ImportError:
        pytest.importorskip("fitz")

    xs = [i * 0.25 for i in range(41)]
    ys1 = [math.exp(-0.18 * x) * math.cos(1.5 * x) + 1.2 for x in xs]
    ys2 = [0.8 * math.exp(-0.12 * x) + 0.4 for x in xs]

    tbl = tg_app.create_table(
        {"Time": xs, "DampedOsc": ys1, "Envelope": ys2},
        designations=["X", "Y", "Y"],
    )
    g = tg_app.plot_xy(tbl, "Time", ["DampedOsc", "Envelope"], style="line", lw=2.0)
    try:
        png_150 = tmp_path / "vis_150dpi.png"
        png_600 = tmp_path / "vis_600dpi.png"
        pdf_vec = tmp_path / "vis_vector.pdf"

        g.export(png_150, dpi=150)
        g.export(png_600, dpi=600)
        g.export(pdf_vec)

        img_150 = load_rgb_image(png_150)
        img_600 = load_rgb_image(png_600)
        img_pdf = load_rgb_image(pdf_vec, pdf_dpi=600)

        # 600 DPI image should be ~4x the dimensions of 150 DPI and match layout closely when downsampled
        ratio_w = img_600.width / max(1, img_150.width)
        ratio_h = img_600.height / max(1, img_150.height)
        assert 3.85 <= ratio_w <= 4.15 and 3.85 <= ratio_h <= 4.15

        dpi_metrics = compare_images(
            img_600,
            img_150,
            resize_to_reference=True,
            channel_tolerance=40,
            diff_heatmap_path=tmp_path / "dpi_150_vs_600_diff.png",
        )
        assert dpi_metrics.ink_ratio_actual > 0.005, "Rendered 600 DPI plot appears blank"
        assert dpi_metrics.rmse < 18.0, f"High-DPI layout drifted from 150 DPI (RMSE={dpi_metrics.rmse:.2f})"
        assert dpi_metrics.differing_pixel_ratio < 0.04, (
            f"Too many pixels differ between 600 DPI and 150 DPI ({dpi_metrics.differing_pixel_ratio:.2%})"
        )

        # Vector PDF rasterized at 600 DPI via PyMuPDF must visually agree with 600 DPI PNG export
        # (only ~1-2% anti-aliased glyph/stroke edge pixels differ between MuPDF and Qt rasterizers)
        pdf_metrics = compare_images(
            img_pdf,
            img_600,
            resize_to_reference=True,
            channel_tolerance=48,
            diff_heatmap_path=tmp_path / "pdf_vs_png600_diff.png",
        )
        assert pdf_metrics.ink_ratio_actual > 0.005, "Vector PDF export appears blank"
        assert pdf_metrics.differing_pixel_ratio < 0.03, (
            f"PDF vector layout diverged from PNG ({pdf_metrics.differing_pixel_ratio:.2%} pixels differ)"
        )
        assert pdf_metrics.rmse < 30.0, f"PDF vector export diverged from PNG (RMSE={pdf_metrics.rmse:.2f})"
    finally:
        tg_app.close_window(g)
        tg_app.close_window(tbl)


def test_export_600dpi_x_axis_title_centered(tg_app, tmp_path: Path) -> None:
    """Verify 600 DPI PNG export centers bottom X-axis title on the plot frame (not shifted)."""
    tbl = tg_app.create_table(
        {"Strain": [0.0, 2.0, 4.0, 6.0, 8.0, 10.0], "Stress": [0.0, 120.0, 240.0, 310.0, 355.0, 380.0]},
        designations=["X", "Y"],
    )
    g = tg_app.plot_xy(tbl, "Strain", ["Stress"], style="line", lw=2.0)
    try:
        g.apply_style_preset("isij_85mm")
        g.set_axis("bottom", title="Tensile Strain / %")
        g.set_axis("left", title="Engineering Stress / MPa")
        png_600 = tmp_path / "x_title_center_600dpi.png"
        g.export(png_600, dpi=600)

        with Image.open(png_600) as im:
            rgb = im.convert("RGB")
            w, h = rgb.size
            px = rgb.load()

            # Locate the bottom X-axis spine as the lowest horizontal dark line spanning >=40% width.
            spine_rows = []
            for y in range(int(h * 0.45), int(h * 0.90)):
                dark_xs = [
                    x for x in range(w)
                    if px[x, y][0] < 90 and px[x, y][1] < 90 and px[x, y][2] < 90
                ]
                if len(dark_xs) >= int(0.40 * w):
                    spine_rows.append((y, dark_xs[0], dark_xs[-1]))

            assert spine_rows, "Could not locate bottom axis spine in 600 DPI export"
            bottom_spine_y, frame_left, frame_right = spine_rows[-1]
            frame_width = float(frame_right - frame_left)
            frame_mid_x = 0.5 * float(frame_left + frame_right)
            assert frame_width > 0.35 * w

            # Segment horizontal ink bands below the spine into ticks row(s) and X-axis title band.
            bands: list[tuple[int, int, list[tuple[int, int]]]] = []
            cur_band_pts: list[tuple[int, int]] = []
            cur_start = -1
            cur_end = -1
            gap = 0
            for y in range(bottom_spine_y + 12, h):
                row_dark = [
                    x for x in range(frame_left - 80 if frame_left >= 80 else 0, min(w, frame_right + 80))
                    if px[x, y][0] < 140 and px[x, y][1] < 140 and px[x, y][2] < 140
                ]
                if len(row_dark) >= 5:
                    if cur_start < 0:
                        cur_start = y
                    cur_end = y
                    gap = 0
                    for x in row_dark:
                        cur_band_pts.append((x, y))
                elif cur_start >= 0:
                    gap += 1
                    if gap >= 8:
                        if len(cur_band_pts) >= 40:
                            bands.append((cur_start, cur_end, cur_band_pts))
                        cur_band_pts = []
                        cur_start = -1
                        cur_end = -1
                        gap = 0
            if cur_start >= 0 and len(cur_band_pts) >= 40:
                bands.append((cur_start, cur_end, cur_band_pts))

            assert len(bands) >= 2, f"Expected tick-number and X-axis title ink bands, found {len(bands)}"
            _, _, title_pts = bands[-1]
            xs_title = [p[0] for p in title_pts]
            centroid_x = sum(xs_title) / float(len(xs_title))
            bbox_mid_x = 0.5 * float(min(xs_title) + max(xs_title))

            assert abs(bbox_mid_x - frame_mid_x) < 0.05 * frame_width, (
                f"X-axis title bbox midpoint shifted horizontally: title_mid={bbox_mid_x:.1f}, "
                f"frame_mid={frame_mid_x:.1f}, frame_w={frame_width:.1f}"
            )
            assert abs(centroid_x - frame_mid_x) < 0.08 * frame_width, (
                f"X-axis title ink centroid shifted horizontally: centroid={centroid_x:.1f}, "
                f"frame_mid={frame_mid_x:.1f}, frame_w={frame_width:.1f}"
            )
    finally:
        tg_app.close_window(g)
        tg_app.close_window(tbl)
