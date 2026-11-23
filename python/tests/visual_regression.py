"""Reusable visual regression & perceptual diff utilities for ToriGraph exports (PNG / PDF).

Supports:
  - Rasterizing vector PDF exports via PyMuPDF (`fitz`) at arbitrary DPI
  - Computing pixel RMSE, channel max delta, non-white ink coverage, and diff ratio
  - Generating red-highlighted diff heatmap PNGs for rapid visual triage
  - Golden baseline comparison (`TORIGRAPH_UPDATE_GOLDENS=1` refreshes baselines)
"""
from __future__ import annotations

import math
import os
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, Tuple

from PIL import Image, ImageChops


@dataclass(frozen=True)
class ImageDiffMetrics:
    width: int
    height: int
    rmse: float
    max_channel_delta: int
    differing_pixel_ratio: float
    ink_ratio_actual: float
    ink_ratio_reference: float
    diff_heatmap_path: Optional[Path] = None


def load_rgb_image(path: Path, pdf_dpi: int = 300) -> Image.Image:
    """Load a PNG/BMP/TIFF or rasterize page 0 of a PDF as a white-composited RGB image."""
    suffix = path.suffix.lower()
    if suffix == ".pdf":
        try:
            import pymupdf
        except ImportError:
            import fitz as pymupdf  # type: ignore[no-redef]

        doc = pymupdf.open(str(path))
        try:
            page = doc.load_page(0)
            pix = page.get_pixmap(dpi=pdf_dpi, alpha=False)
            return Image.frombytes("RGB", (pix.width, pix.height), pix.samples)
        finally:
            doc.close()

    with Image.open(path) as raw:
        if raw.mode in ("RGBA", "LA") or (raw.mode == "P" and "transparency" in raw.info):
            rgba = raw.convert("RGBA")
            bg = Image.new("RGBA", rgba.size, (255, 255, 255, 255))
            return Image.alpha_composite(bg, rgba).convert("RGB")
        return raw.convert("RGB")


def compute_ink_ratio(img: Image.Image, white_threshold: int = 248) -> float:
    """Fraction of pixels whose minimum RGB channel is darker than `white_threshold`."""
    rgb = img.convert("RGB")
    total = rgb.width * rgb.height
    if total == 0:
        return 0.0
    raw = rgb.tobytes()
    ink = 0
    for i in range(0, len(raw), 3):
        if raw[i] < white_threshold or raw[i + 1] < white_threshold or raw[i + 2] < white_threshold:
            ink += 1
    return ink / float(total)


def compare_images(
    actual: Image.Image,
    reference: Image.Image,
    *,
    resize_to_reference: bool = True,
    channel_tolerance: int = 16,
    diff_heatmap_path: Optional[Path] = None,
) -> ImageDiffMetrics:
    """Compare two RGB images and optionally write a red-highlighted diff heatmap PNG."""
    ref_rgb = reference.convert("RGB")
    act_rgb = actual.convert("RGB")
    if act_rgb.size != ref_rgb.size:
        if not resize_to_reference:
            raise ValueError(f"Image size mismatch: actual={act_rgb.size} vs reference={ref_rgb.size}")
        act_rgb = act_rgb.resize(ref_rgb.size, Image.Resampling.BOX)

    diff = ImageChops.difference(act_rgb, ref_rgb)
    w, h = ref_rgb.size
    total_px = max(1, w * h)

    diff_raw = diff.tobytes()
    sq_sum = 0.0
    max_delta = 0
    diff_px = 0
    for i in range(0, len(diff_raw), 3):
        dr = diff_raw[i]
        dg = diff_raw[i + 1]
        db = diff_raw[i + 2]
        sq_sum += float(dr * dr + dg * dg + db * db)
        m = dr if dr >= dg else dg
        if db > m:
            m = db
        if m > max_delta:
            max_delta = m
        if m > channel_tolerance:
            diff_px += 1

    rmse = math.sqrt(sq_sum / float(total_px * 3))
    diff_ratio = diff_px / float(total_px)

    if diff_heatmap_path is not None:
        diff_heatmap_path.parent.mkdir(parents=True, exist_ok=True)
        gray_raw = ref_rgb.convert("L").tobytes()
        heat_buf = bytearray(w * h * 3)
        p_idx = 0
        for g_val in gray_raw:
            dr = diff_raw[p_idx]
            dg = diff_raw[p_idx + 1]
            db = diff_raw[p_idx + 2]
            m = max(dr, dg, db)
            if m > channel_tolerance:
                c = max(0, 60 - m // 2)
                heat_buf[p_idx] = 255
                heat_buf[p_idx + 1] = c
                heat_buf[p_idx + 2] = c
            else:
                faded = 180 + (g_val * 75) // 255
                heat_buf[p_idx] = faded
                heat_buf[p_idx + 1] = faded
                heat_buf[p_idx + 2] = faded
            p_idx += 3
        Image.frombytes("RGB", (w, h), bytes(heat_buf)).save(diff_heatmap_path)

    return ImageDiffMetrics(
        width=w,
        height=h,
        rmse=rmse,
        max_channel_delta=max_delta,
        differing_pixel_ratio=diff_ratio,
        ink_ratio_actual=compute_ink_ratio(act_rgb),
        ink_ratio_reference=compute_ink_ratio(ref_rgb),
        diff_heatmap_path=diff_heatmap_path,
    )


def assert_matches_golden(
    actual_path: Path,
    golden_path: Path,
    *,
    diff_dir: Optional[Path] = None,
    max_rmse: float = 4.0,
    max_diff_ratio: float = 0.005,
    channel_tolerance: int = 16,
    pdf_dpi: int = 300,
) -> Tuple[bool, Optional[ImageDiffMetrics]]:
    """Compare `actual_path` against `golden_path` (or write `golden_path` when updating)."""
    actual_img = load_rgb_image(actual_path, pdf_dpi=pdf_dpi)
    if os.environ.get("TORIGRAPH_UPDATE_GOLDENS", "").strip() == "1" or not golden_path.exists():
        if os.environ.get("TORIGRAPH_UPDATE_GOLDENS", "").strip() == "1":
            golden_path.parent.mkdir(parents=True, exist_ok=True)
            actual_img.save(golden_path)
            return True, None
        raise FileNotFoundError(
            f"Golden baseline missing: {golden_path} (re-run with TORIGRAPH_UPDATE_GOLDENS=1 to create)"
        )

    ref_img = load_rgb_image(golden_path, pdf_dpi=pdf_dpi)
    diff_out = (diff_dir / f"{golden_path.stem}_diff.png") if diff_dir else None
    metrics = compare_images(
        actual_img,
        ref_img,
        resize_to_reference=False,
        channel_tolerance=channel_tolerance,
        diff_heatmap_path=diff_out,
    )
    ok = metrics.rmse <= max_rmse and metrics.differing_pixel_ratio <= max_diff_ratio
    if not ok:
        raise AssertionError(
            f"Visual regression vs {golden_path.name}: RMSE={metrics.rmse:.2f} (max {max_rmse}), "
            f"diff_ratio={metrics.differing_pixel_ratio:.4%} (max {max_diff_ratio:.4%}), "
            f"heatmap={metrics.diff_heatmap_path}"
        )
    return True, metrics
