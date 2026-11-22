#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ToriGraph Live Real-Data End-to-End Python Automation Verification Suite
========================================================================
Exercises the live ToriGraph C++ engine over TCP JSON-RPC 2.0 via `import torigraph as tg`
(`with tg.launch(headless=True, language="ja") as app:`) across all 7 scientific domains:

  0. Session Handshake, Japanese Mode & High-Level SDK Proxy Contract
  1. Al-Zn-Mg Thermal Desorption Spectroscopy (TDS) 2-Peak Deconvolution
  2. Tensile Stress-Strain Curve + ISIJ 85mm & Slide 28pt Presets + Axis Break +
     Zoom Inset + Reference Line/Band + 600 DPI Multi-Format Export (.png/.pdf/.svg/.emf)
  3. Nonlinear Fitting Across Built-in Origin Models, Global Fit, Piecewise, Robust,
     York XY-Error, Circle ODR, Logistic Regression, MLR & Model/Dataset Comparison
  4. Materials Science Domain Workflows (Arrhenius Ea, Tauc Direct Eg, EIS Randles+CPE)
  5. 1D & 2D Signal Processing + Matrix Heatmap (Viridis Colorbar) / Contour / 3D Surface
  6. Parametric, Nonparametric, Survival, Reliability (3-Param Censored Weibull) &
     Multivariate Statistics + Gadgets + Curve Math + Digitizer + .sciprj Round-Trip +
     Local HTTP Mock GitHub Release Auto-Updater Check
  7. Headless CLI `--run-python <script.py> --headless` Execution
"""

from __future__ import annotations

import gc
import gzip
import hashlib
import http.server
import json
import math
import os
import random
import struct
import subprocess
import sys
import tempfile
import threading
import time
import traceback
import zlib
from pathlib import Path
from typing import Any, Callable, Dict, List, Tuple

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
PYTHON_SDK_ROOT = REPO_ROOT / "python"
if str(PYTHON_SDK_ROOT) not in sys.path:
    sys.path.insert(0, str(PYTHON_SDK_ROOT))

import torigraph as tg  # noqa: E402


# ---------------------------------------------------------------------------
# Pure-Stdlib PNG Helpers (IHDR/pHYs/IDAT inspection + synthetic Digitizer PNG)
# ---------------------------------------------------------------------------

def parse_png_metadata_and_pixels(png_path: Path) -> Dict[str, Any]:
    """Parse PNG IHDR, pHYs, and decode IDAT scanlines via stdlib zlib."""
    raw = png_path.read_bytes()
    if len(raw) < 33 or raw[:8] != b"\x89PNG\r\n\x1a\n":
        raise AssertionError(f"Invalid PNG signature in {png_path}")

    pos = 8
    width = height = bit_depth = color_type = 0
    ppm_x = ppm_y = 0
    idat_chunks: List[bytes] = []

    while pos + 8 <= len(raw):
        length = struct.unpack(">I", raw[pos : pos + 4])[0]
        ctype = raw[pos + 4 : pos + 8]
        cdata = raw[pos + 8 : pos + 8 + length]
        pos += 12 + length
        if ctype == b"IHDR":
            width, height, bit_depth, color_type, _, _, _ = struct.unpack(">IIBBBBB", cdata[:13])
        elif ctype == b"pHYs" and len(cdata) >= 9:
            px, py, unit = struct.unpack(">IIB", cdata[:9])
            if unit == 1:
                ppm_x, ppm_y = px, py
        elif ctype == b"IDAT":
            idat_chunks.append(cdata)
        elif ctype == b"IEND":
            break

    decompressed = zlib.decompress(b"".join(idat_chunks))
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(color_type, 4)
    bpp = max(1, (channels * bit_depth) // 8)
    stride = width * bpp

    # Reconstruct ~60 sampled interior rows (or full image if small) to verify non-blank content
    prev_row = bytearray(stride)
    cur_row = bytearray(stride)
    non_white_interior = 0
    hud_blue_accent_px = 0
    x_lo, x_hi = int(width * 0.15), int(width * 0.85)
    y_lo, y_hi = int(height * 0.15), int(height * 0.85)

    offset = 0
    for y in range(height):
        if offset >= len(decompressed):
            break
        ftype = decompressed[offset]
        offset += 1
        scan = decompressed[offset : offset + stride]
        offset += stride
        if ftype == 0:
            cur_row[:] = scan
        elif ftype == 1:
            for i in range(stride):
                a = cur_row[i - bpp] if i >= bpp else 0
                cur_row[i] = (scan[i] + a) & 0xFF
        elif ftype == 2:
            for i in range(stride):
                cur_row[i] = (scan[i] + prev_row[i]) & 0xFF
        elif ftype == 3:
            for i in range(stride):
                a = cur_row[i - bpp] if i >= bpp else 0
                b = prev_row[i]
                cur_row[i] = (scan[i] + ((a + b) >> 1)) & 0xFF
        elif ftype == 4:
            for i in range(stride):
                a = cur_row[i - bpp] if i >= bpp else 0
                b = prev_row[i]
                c = prev_row[i - bpp] if i >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                cur_row[i] = (scan[i] + pr) & 0xFF

        if (y % 4 == 0) and color_type in (2, 6):
            for x in range(0, width, 4):
                idx = x * bpp
                r, g_c, b_c = cur_row[idx], cur_row[idx + 1], cur_row[idx + 2]
                if x_lo <= x <= x_hi and y_lo <= y <= y_hi:
                    if r < 245 or g_c < 245 or b_c < 245:
                        non_white_interior += 1
                # #2563EB HUD active layer pill signature check in top-left corner
                if x < 180 and y < 40 and (b_c - r > 140 and r < 70 and b_c > 200):
                    hud_blue_accent_px += 1

        prev_row, cur_row = cur_row, prev_row

    return {
        "width": width,
        "height": height,
        "bit_depth": bit_depth,
        "color_type": color_type,
        "ppm_x": ppm_x,
        "ppm_y": ppm_y,
        "non_white_interior": non_white_interior,
        "hud_blue_accent_px": hud_blue_accent_px,
    }


def write_synthetic_digitizer_png(
    png_path: Path,
    width: int = 500,
    height: int = 400,
    px_x1: int = 60,
    px_x2: int = 460,
    py_y1: int = 350,
    py_y2: int = 50,
) -> None:
    """Write a deterministic 24-bit RGB PNG with black axes and pure red curve y = 0.5*x + 15."""
    buf = bytearray([255] * (width * height * 3))

    def set_px(x: int, y: int, r: int, g: int, b: int) -> None:
        if 0 <= x < width and 0 <= y < height:
            i = (y * width + x) * 3
            buf[i] = r
            buf[i + 1] = g
            buf[i + 2] = b

    # Black axes frame (ignored by red color tracer)
    for y in range(py_y2, py_y1 + 1):
        set_px(px_x1, y, 0, 0, 0)
    for x in range(px_x1, px_x2 + 1):
        set_px(x, py_y1, 0, 0, 0)

    # Pure red 3px thick curve y = 0.5 * x + 15.0 for x in [10, 90] on [0..100] x [0..100]
    for px in range(px_x1 + 40, px_x1 + 361):
        x_val = (px - px_x1) / float(px_x2 - px_x1) * 100.0
        y_val = 0.5 * x_val + 15.0
        py_f = py_y1 - (y_val / 100.0) * float(py_y1 - py_y2)
        py_c = int(round(py_f))
        for dy in (-1, 0, 1):
            set_px(px, py_c + dy, 255, 0, 0)

    def png_chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    raw_rows = bytearray()
    row_bytes = width * 3
    for y in range(height):
        raw_rows.append(0)  # Filter type 0 (None)
        raw_rows.extend(buf[y * row_bytes : (y + 1) * row_bytes])

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    idat = zlib.compress(bytes(raw_rows), 6)
    png_bytes = (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", ihdr)
        + png_chunk(b"IDAT", idat)
        + png_chunk(b"IEND", b"")
    )
    png_path.write_bytes(png_bytes)


def pseudo_voigt(x: float, amp: float, xc: float, fwhm: float, mu: float = 0.35) -> float:
    w = max(1e-9, fwhm)
    dx = (x - xc) / (0.5 * w)
    lor = 1.0 / (1.0 + dx * dx)
    gau = math.exp(-math.log(2.0) * dx * dx)
    return amp * (mu * lor + (1.0 - mu) * gau)


def populate_table_rows(
    tbl: tg.TableProxy, rows: List[List[float]], col_names: List[str]
) -> None:
    """Populate a TableProxy via high-level TableProxy.set_data_dict."""
    n_cols = len(col_names)
    data_map = {col_names[c]: [row[c] for row in rows] for c in range(n_cols)}
    tbl.set_data_dict(data_map)


def extract_table_rows(tbl: tg.TableProxy) -> List[List[float]]:
    info = tbl.info()
    cols = info.get("columns", [])
    col_vecs = [tbl.get_column(i) for i in range(len(cols))]
    if not col_vecs:
        return []
    return [[col_vecs[c][r] for c in range(len(col_vecs))] for r in range(len(col_vecs[0]))]


def assert_graph_arial(app: tg.Session, graph_name: str, context: str) -> Dict[str, Any]:
    info = app.call("graph.get_info", graph=graph_name)
    if not info.get("all_fonts_arial", False):
        raise AssertionError(f"[{context}] graph '{graph_name}' violated Arial font invariant: {info}")
    for layer in info.get("layers", []):
        for ax_key, ax_obj in layer.get("axes", {}).items():
            if ax_obj.get("title_font_family") != "Arial" or ax_obj.get("numbers_font_family") != "Arial":
                raise AssertionError(
                    f"[{context}] graph '{graph_name}' axis '{ax_key}' non-Arial: {ax_obj}"
                )
    return info


# ---------------------------------------------------------------------------
# Test Runner Harness
# ---------------------------------------------------------------------------

class E2ERunner:
    def __init__(self) -> None:
        self.passed: List[str] = []
        self.failed: List[Tuple[str, str]] = []

    def check(self, name: str, fn: Callable[[], None]) -> None:
        try:
            fn()
            self.passed.append(name)
            print(f"[PASS] {name}")
        except Exception as exc:
            tb = traceback.format_exc()
            self.failed.append((name, f"{exc}\n{tb}"))
            print(f"[FAIL] {name}: {exc}", file=sys.stderr)


# ---------------------------------------------------------------------------
# Workflow Sections
# ---------------------------------------------------------------------------

def run_section_0_sdk_proxy_contract(runner: E2ERunner, app: tg.Session, tmp_dir: Path) -> None:
    def _handshake_and_ja() -> None:
        ping_res = app.ping()
        assert isinstance(ping_res, dict) and ping_res.get("pong") is True, f"ping() failed: {ping_res}"
        assert app.get_language() == "ja", f"Expected language='ja', got {app.get_language()!r}"
        methods = app.supported_methods()
        assert len(methods) >= 120, f"Expected >= 120 RPC methods, got {len(methods)}"
        app.set_focus_hud(True)
        assert app.get_focus_hud() is True
        app.set_focus_hud(False)
        assert app.get_focus_hud() is False
        assert app.set_workspace_mode("split").get("mode") == "split"
        assert app.set_workspace_mode("tabs").get("mode") == "tabs"

    runner.check("0.1 Session handshake, JA language & workspace/HUD RPC", _handshake_and_ja)

    # High-level TableProxy / MatrixProxy / GraphProxy contract checks
    tbl = app.create_table(rows=5, cols=3, name="ProxyContractTbl")

    def _table_set_get_column_and_dict_order() -> None:
        tbl.set_column(0, [3.0, 1.0, 5.0, 2.0, 4.0])
        tbl.set_column(1, [30.0, 10.0, 50.0, 20.0, 40.0])
        tbl.set_column(2, [0.3, 0.1, 0.5, 0.2, 0.4])
        got = tbl.get_column(1)
        assert len(got) == 5 and abs(got[0] - 30.0) < 1e-9 and abs(got[2] - 50.0) < 1e-9, (
            f"TableProxy.get_column returned {got}"
        )
        # Verify TableProxy.set_data_dict preserves Python dict insertion order (not alphabetical)
        tbl_dict = app.create_table(rows=2, cols=2, name="ProxyDictOrderTbl")
        tbl_dict.set_data_dict({"Z_Temp": [300.0, 400.0], "A_Rate": [1.5, 2.5]})
        col0_name = tbl_dict.info()["columns"][0]["name"]
        assert col0_name == "Z_Temp", (
            f"TableProxy.set_data_dict reordered columns alphabetically (col 0 = {col0_name!r}, expected 'Z_Temp')"
        )

    runner.check("0.2 TableProxy.set_column, get_column & set_data_dict order", _table_set_get_column_and_dict_order)

    def _table_mask_and_sort_and_csv() -> None:
        populate_table_rows(
            tbl,
            [[3.0, 30.0, 0.3], [1.0, 10.0, 0.1], [5.0, 50.0, 0.5], [2.0, 20.0, 0.2], [4.0, 40.0, 0.4]],
            ["C1", "C2", "C3"],
        )
        tbl.mask_rows([1, 3])
        masked = tbl.get_masked_rows()
        assert sorted(masked) == [1, 3], f"TableProxy.get_masked_rows returned {masked}"
        tbl.clear_masks()
        assert tbl.get_masked_rows() == []
        tbl.sort(by_col=0, ascending=True, separately=False)
        rows_after = extract_table_rows(tbl)
        assert abs(rows_after[0][0] - 1.0) < 1e-9 and abs(rows_after[4][1] - 50.0) < 1e-9, (
            f"TableProxy.sort did not sort companion columns: {rows_after}"
        )
        csv_out = tmp_dir / "proxy_contract.csv"
        tbl.export_csv(csv_out, separator=",", with_labels=True)
        txt = csv_out.read_text(encoding="utf-8")
        lines = [ln for ln in txt.splitlines() if ln.strip()]
        assert len(lines) == 6, f"Expected header + 5 data rows in CSV, got {len(lines)} lines"

    runner.check("0.3 TableProxy.get_masked_rows, sort & export_csv", _table_mask_and_sort_and_csv)

    def _matrix_and_graph_proxies() -> None:
        proxy_failures: List[str] = []

        # MatrixProxy.fft2d(shift=False): constant grid DC component must stay at [0][0], not [4][4]
        try:
            m = app.create_matrix(rows=8, cols=8, name="ProxyFftMat")
            m.set_data([[2.0 for _ in range(8)] for _ in range(8)])
            m_fft = m.fft2d(inverse=False, shift=False)
            mag_mat = app.matrix(m_fft["name"])
            mag_grid = mag_mat.get_data()
            if mag_grid[0][0] <= mag_grid[4][4]:
                proxy_failures.append(
                    f"MatrixProxy.fft2d(shift=False) shifted DC to center ([0][0]={mag_grid[0][0]}, [4][4]={mag_grid[4][4]})"
                )
        except Exception as exc:
            proxy_failures.append(f"MatrixProxy.fft2d: {exc}")

        # Session.plot_ternary
        try:
            tern_tbl = app.create_table(rows=4, cols=3, name="ProxyTernaryTbl")
            populate_table_rows(
                tern_tbl,
                [[0.7, 0.2, 0.1], [0.2, 0.6, 0.2], [0.1, 0.3, 0.6], [0.34, 0.33, 0.33]],
                ["A", "B", "C"],
            )
            g_tern = app.plot_ternary(tern_tbl, a_col=0, b_col=1, c_col=2)
            assert_graph_arial(app, g_tern.name, "plot_ternary")
        except Exception as exc:
            proxy_failures.append(f"Session.plot_ternary: {exc}")

        # GraphProxy methods: add_linked_layer, set_aspect_ratio, set_stack_offsets, mask_region
        stk_tbl = app.create_table(rows=6, cols=3, name="ProxyStackTbl")
        populate_table_rows(
            stk_tbl,
            [[float(i), float(i * 2), float(i * 2 + 1)] for i in range(6)],
            ["X", "Y1", "Y2"],
        )
        g_stk = app.plot_xy(stk_tbl, x_col=0, y_cols=[1, 2], style="line_symbol")

        try:
            ll_res = g_stk.add_linked_layer(link_x=True, right_y=False)
            # right_y=False should pass link_y=False to C++
            if ll_res.get("link_y") is not False and len(g_stk.info().get("layers", [])) < 2:
                proxy_failures.append(f"GraphProxy.add_linked_layer unexpected response: {ll_res}")
        except Exception as exc:
            proxy_failures.append(f"GraphProxy.add_linked_layer: {exc}")

        try:
            g_stk.set_aspect_ratio(enabled=False, ratio=1.0)
            ar_off = g_stk.info()
            g_stk.set_aspect_ratio(enabled=True, ratio=1.0)
            ar_on = g_stk.info()
            if ar_off.get("keep_aspect_ratio") is not False or ar_on.get("keep_aspect_ratio") is not True:
                proxy_failures.append(
                    f"GraphProxy.set_aspect_ratio did not toggle keep_aspect_ratio: off={ar_off.get('keep_aspect_ratio')}, on={ar_on.get('keep_aspect_ratio')}"
                )
        except Exception as exc:
            proxy_failures.append(f"GraphProxy.set_aspect_ratio: {exc}")

        try:
            so_res = g_stk.set_stack_offsets(mode=1, y_offset_pct=25.0, x_offset_pct=0.0)
            if not so_res.get("ok"):
                proxy_failures.append(f"GraphProxy.set_stack_offsets failed: {so_res}")
        except Exception as exc:
            proxy_failures.append(f"GraphProxy.set_stack_offsets: {exc}")

        try:
            mr = g_stk.mask_region(0.5, 2.5, -10.0, 50.0, unmask=False)
            mr_un = g_stk.mask_region(0.5, 2.5, -10.0, 50.0, unmask=True)
            masked_left = app.call("table.get_masked_rows", table=stk_tbl.name, col=1).get("rows", [])
            if masked_left:
                proxy_failures.append(
                    f"GraphProxy.mask_region(unmask=True) left rows masked: {masked_left} (resp={mr_un})"
                )
        except Exception as exc:
            proxy_failures.append(f"GraphProxy.mask_region: {exc}")

        # Session.fit_implicit_odr(shape="ellipse")
        try:
            t_el = app.create_table(rows=24, cols=2, name="ProxyEllipseTbl")
            populate_table_rows(
                t_el,
                [
                    [4.0 * math.cos(2.0 * math.pi * k / 24.0), 2.0 * math.sin(2.0 * math.pi * k / 24.0)]
                    for k in range(24)
                ],
                ["X", "Y"],
            )
            g_el = app.plot_xy(t_el, x_col=0, y_cols=[1], style="scatter")
            el_res = app.fit_implicit_odr(graph=g_el.name, shape="ellipse")
            if el_res.get("model") != "ellipse":
                proxy_failures.append(
                    f"Session.fit_implicit_odr(shape='ellipse') fitted model={el_res.get('model')!r} instead of 'ellipse'"
                )
        except Exception as exc:
            proxy_failures.append(f"Session.fit_implicit_odr: {exc}")

        assert not proxy_failures, "SDK proxy contract mismatches:\n  - " + "\n  - ".join(proxy_failures)

    runner.check(
        "0.4 MatrixProxy.fft2d, plot_ternary, GraphProxy methods & fit_implicit_odr",
        _matrix_and_graph_proxies,
    )


def run_workflow_1_tds_deconvolution(runner: E2ERunner, app: tg.Session) -> None:
    def _tds_workflow() -> None:
        rng = random.Random(20261112)
        n_pts = 241
        rows: List[List[float]] = []
        tp1_true, tp2_true = 415.0, 505.0
        for i in range(n_pts):
            t_k = 300.0 + i * 1.5  # 300 .. 660 K
            baseline = 1.2 + 0.008 * (t_k - 300.0) + 1.5e-5 * ((t_k - 300.0) ** 2)
            p1 = pseudo_voigt(t_k, 28.0, tp1_true, 34.0, mu=0.35)
            p2 = pseudo_voigt(t_k, 42.0, tp2_true, 40.0, mu=0.35)
            noise = rng.gauss(0.0, 0.12)
            rows.append([t_k, baseline + p1 + p2 + noise])

        tbl = app.create_table(rows=n_pts, cols=2, name="AlZnMg_TDS")
        populate_table_rows(tbl, rows, ["TempK", "DesorpRate"])
        tbl.set_meta(0, name="TempK", long_name="Temperature", units="K", plot_designation="X")
        tbl.set_meta(1, name="DesorpRate", long_name="Desorption Rate", units="10^-3 wt.ppm/s", plot_designation="Y")

        g = app.plot_xy(tbl, x_col="TempK", y_cols=["DesorpRate"], style="line", graph_name="TDS_Deconv_Graph")
        res = app.analyze_peaks(
            graph=g.name,
            profile="PseudoVoigt",
            baseline_mode="als",
            num_peaks=2,
            integrate=True,
            create_summary=True,
            plot_peaks=True,
            plot_baseline=True,
        )
        assert res.get("ok") is True, f"analyze_peaks failed: {res}"
        assert res.get("peak_count") == 2, f"Expected 2 deconvolved TDS peaks, got {res.get('peak_count')}"
        peaks = sorted(res["peaks"], key=lambda p: p["center"])
        c1, c2 = peaks[0]["center"], peaks[1]["center"]
        r2 = res.get("r2", 0.0)
        assert abs(c1 - tp1_true) < 2.5, f"Peak 1 center {c1:.2f} K too far from {tp1_true} K"
        assert abs(c2 - tp2_true) < 2.5, f"Peak 2 center {c2:.2f} K too far from {tp2_true} K"
        assert r2 > 0.995, f"TDS deconvolution R^2={r2:.5f} <= 0.995"
        assert peaks[1]["area"] > peaks[0]["area"] > 100.0, f"Unexpected peak areas: {peaks}"
        assert "integration_table" in res and "summary_table" in res

        g_info = assert_graph_arial(app, g.name, "Workflow 1 TDS")
        curve_cnt = g_info["layers"][0]["curve_count"]
        assert curve_cnt >= 5, f"Expected raw + total fit + 2 components + baseline (>=5 curves), got {curve_cnt}"
        print(
            f"  -> TDS Deconv: Tp1={c1:.2f} K, Tp2={c2:.2f} K, R^2={r2:.5f}, curves={curve_cnt}"
        )

    runner.check("1. Al-Zn-Mg TDS 2-Peak PseudoVoigt + ALS Baseline Deconvolution", _tds_workflow)


def run_workflow_2_tensile_presets_export(
    runner: E2ERunner, app: tg.Session, tmp_dir: Path
) -> Dict[str, str]:
    saved_refs: Dict[str, str] = {}

    def _tensile_workflow() -> None:
        # Synthetic 7075-T6 / DP steel tensile stress-strain curve (elastic + Hollomon work hardening)
        e_mod = 71000.0
        sigma_y = 480.0
        eps_y = sigma_y / e_mod
        k_hol, n_hol = 820.0, 0.14
        rows: List[List[float]] = []
        for i in range(160):
            eps = 0.22 * (i / 159.0)
            if eps <= eps_y:
                sig = e_mod * eps
            else:
                eps_p = eps - eps_y
                sig = sigma_y + (k_hol * ((eps_p + 0.002) ** n_hol) - k_hol * (0.002 ** n_hol))
            rows.append([eps, sig])

        tbl = app.create_table(rows=len(rows), cols=2, name="TensileHollomon")
        populate_table_rows(tbl, rows, ["Strain", "Stress"])
        tbl.set_meta(0, name="Strain", long_name="Engineering Strain", units="-", plot_designation="X")
        tbl.set_meta(1, name="Stress", long_name="Engineering Stress", units="MPa", plot_designation="Y")

        g = app.plot_xy(tbl, x_col="Strain", y_cols=["Stress"], style="line", graph_name="TensileISIJGraph")
        g.apply_style_preset("isij_85mm")
        g.set_axis_break("x", enabled=True, start=0.04, end=0.12, position_pct=45.0)
        g.add_inset(x_min=0.0, x_max=0.012, y_min=0.0, y_max=540.0)
        g.add_reference_line(axis="y", value=485.0, label="0.2% Proof Stress", color="#DC2626")
        g.add_shaded_band(y_min=470.0, y_max=500.0, color="#2563EB", alpha=40, title="YieldBand")

        info_isij = assert_graph_arial(app, g.name, "Workflow 2 ISIJ 85mm")
        assert abs(info_isij.get("page_width", 0.0) - 85.0) < 0.5
        assert abs(info_isij.get("page_height", 0.0) - 85.0) < 0.5
        assert len(info_isij.get("layers", [])) >= 2, "Zoom inset layer missing"

        png_path = tmp_dir / "tensile_isij_600dpi.png"
        pdf_path = tmp_dir / "tensile_isij_600dpi.pdf"
        svg_path = tmp_dir / "tensile_isij_600dpi.svg"
        emf_path = tmp_dir / "tensile_isij_600dpi.emf"

        for p in (png_path, pdf_path, svg_path, emf_path):
            exp = g.export(p, dpi=600)
            assert exp.get("ok") is True and p.is_file() and p.stat().st_size > 2000, (
                f"Export failed or trivial file for {p.suffix}: {exp}"
            )

        png_meta = parse_png_metadata_and_pixels(png_path)
        expected_px = round(85.0 / 25.4 * 600.0)  # 2008 px
        expected_ppm = round(600.0 / 0.0254)      # 23622 px/m
        assert abs(png_meta["width"] - expected_px) <= 12 and abs(png_meta["height"] - expected_px) <= 12, (
            f"PNG size {png_meta['width']}x{png_meta['height']} != expected ~{expected_px}x{expected_px}"
        )
        assert abs(png_meta["ppm_x"] - expected_ppm) <= 5, f"PNG pHYs ppm_x={png_meta['ppm_x']} != {expected_ppm}"
        assert png_meta["non_white_interior"] >= 50, "Exported 600 DPI PNG interior appears blank!"
        assert png_meta["hud_blue_accent_px"] == 0, "Exported PNG leaked #2563EB HUD layer pill!"

        assert pdf_path.read_bytes()[:5] == b"%PDF-", "Invalid PDF header"
        svg_txt = svg_path.read_text(encoding="utf-8", errors="replace")
        assert "<svg" in svg_txt and "Arial" in svg_txt, "Exported SVG missing <svg> or Arial font"
        emf_bytes = emf_path.read_bytes()
        assert len(emf_bytes) >= 88 and emf_bytes[40:44] == b" EMF", "Invalid Windows EMF header signature"

        # Test Slide 28pt preset on a clone so TensileISIJGraph stays 85mm for .sciprj round-trip!
        cloned = app.clone_window(g.name)
        slide_name = cloned.get("name") or g.name
        g_slide = app.graph(slide_name)
        info_slide = g_slide.apply_style_preset("slide_28pt")
        assert_graph_arial(app, g_slide.name, "Workflow 2 Slide 28pt")
        x_ax = info_slide["layers"][0]["axes"]["x"]
        assert x_ax.get("title_font_size") == 28, f"Expected 28pt slide axis title font, got {x_ax}"

        saved_refs["tensile_graph"] = g.name
        saved_refs["tensile_table"] = tbl.name
        print(
            f"  -> Exports verified: PNG={png_meta['width']}x{png_meta['height']}px ({png_path.stat().st_size} B), "
            f"PDF={pdf_path.stat().st_size} B, SVG={svg_path.stat().st_size} B, EMF={emf_path.stat().st_size} B"
        )

    runner.check(
        "2. Tensile Hollomon + ISIJ 85mm & Slide 28pt + Axis Break + Inset + 600 DPI Exports",
        _tensile_workflow,
    )
    return saved_refs


def run_workflow_3_nonlinear_and_regression(runner: E2ERunner, app: tg.Session) -> None:
    def _models_and_boltzmann() -> None:
        models = app.list_fit_models()
        assert len(models) >= 58, f"Expected >= 58 built-in fit models, got {len(models)}"

        # Boltzmann sigmoidal curve: y = A2 + (A1 - A2) / (1 + exp((x - x0)/dx))
        a1_t, a2_t, x0_t, dx_t = 12.0, 96.0, 50.0, 5.5
        rows = [
            [float(x), a2_t + (a1_t - a2_t) / (1.0 + math.exp((x - x0_t) / dx_t))]
            for x in range(0, 101, 2)
        ]
        tbl = app.create_table(rows=len(rows), cols=2, name="BoltzmannTbl")
        populate_table_rows(tbl, rows, ["X", "Y"])
        g = app.plot_xy(tbl, x_col=0, y_cols=[1], style="scatter")
        fit = app.fit_nonlinear(graph=g.name, model="Boltzmann")
        assert fit.get("converged") is True and fit.get("r2", 0.0) > 0.9999, f"Boltzmann fit failed: {fit}"
        p = fit["params"]
        assert abs(p["x0"] - x0_t) < 0.05 and abs(p["dx"] - dx_t) < 0.05

    runner.check("3.1 Built-in Origin Fit Models (>=58) & Boltzmann Sigmoidal Fit", _models_and_boltzmann)

    def _global_piecewise_robust_york_odr() -> None:
        # Global shared-tau fit across 2 exponential decay curves (shared t1 = 18.0)
        rows_gl = [
            [float(x), 5.0 + 80.0 * math.exp(-x / 18.0), 12.0 + 140.0 * math.exp(-x / 18.0)]
            for x in range(0, 91, 3)
        ]
        t_gl = app.create_table(rows=len(rows_gl), cols=3, name="GlobalExpTbl")
        populate_table_rows(t_gl, rows_gl, ["Time", "DecayA", "DecayB"])
        g_gl = app.plot_xy(t_gl, x_col=0, y_cols=[1, 2], style="line_symbol")
        gl_res = app.fit_global(graph=g_gl.name, model="ExpDec1", shared=["t1"])
        assert gl_res.get("converged") is True and "summary" in gl_res, f"Global fit failed: {gl_res}"

        # Dataset comparison F-test on the same 2-curve graph
        dc_res = app.compare_datasets(graph=g_gl.name, model="ExpDec1")
        assert dc_res.get("ok") is True and "f_statistic" in dc_res, f"compare_datasets failed: {dc_res}"

        # 2-segment Piecewise linear breakpoint fit (breakpoint at x = 40.0, slopes 2.0 and 0.5)
        rows_pw = [
            [float(x), (2.0 * x + 5.0) if x <= 40 else (85.0 + 0.5 * (x - 40.0))]
            for x in range(0, 81, 2)
        ]
        t_pw = app.create_table(rows=len(rows_pw), cols=2, name="PiecewiseTbl")
        populate_table_rows(t_pw, rows_pw, ["X", "Y"])
        g_pw = app.plot_xy(t_pw, x_col=0, y_cols=[1], style="line_symbol")
        pw_res = app.fit_piecewise(graph=g_pw.name, segments=2)
        assert pw_res.get("converged") is True, f"Piecewise fit failed: {pw_res}"
        assert abs(pw_res["breakpoint"] - 40.0) < 0.8, f"Piecewise breakpoint={pw_res['breakpoint']} != 40.0"
        assert abs(pw_res["slopes"][0] - 2.0) < 0.05 and abs(pw_res["slopes"][1] - 0.5) < 0.05

        # Huber & Tukey Bisquare Robust fit on linear data y = 3.2*x - 4.0 with massive outlier y=9500
        rows_rb = [[float(x), 3.2 * x - 4.0] for x in range(25)]
        rows_rb[12][1] = 9500.0
        t_rb = app.create_table(rows=len(rows_rb), cols=2, name="RobustOutlierTbl")
        populate_table_rows(t_rb, rows_rb, ["X", "Y"])
        g_rb = app.plot_xy(t_rb, x_col=0, y_cols=[1], style="scatter")
        ols_bad = app.fit_linear(graph=g_rb.name)
        hub_res = app.fit_robust(graph=g_rb.name, method="huber")
        bis_res = app.fit_robust(graph=g_rb.name, method="bisquare")
        assert abs(bis_res["slope"] - 3.2) < 0.05 and abs(bis_res["intercept"] - (-4.0)) < 0.25, (
            f"Bisquare failed outlier resistance: {bis_res}"
        )
        assert abs(hub_res["slope"] - 3.2) < abs(ols_bad["slope"] - 3.2), "Huber not closer than OLS"

        # York XY-error regression
        rng = random.Random(42)
        rows_yk = []
        for i in range(20):
            xv = float(i + 1)
            sx = 0.15 + 0.02 * (i % 3)
            sy = 0.35 + 0.05 * (i % 4)
            rows_yk.append([xv + rng.gauss(0, 0.05), 2.4 * xv + 1.5 + rng.gauss(0, 0.12), sx, sy])
        t_yk = app.create_table(rows=len(rows_yk), cols=4, name="YorkXYTbl")
        populate_table_rows(t_yk, rows_yk, ["X", "Y", "dX", "dY"])
        g_yk = app.plot_xy(t_yk, x_col="X", y_cols=["Y"], x_err_col="dX", y_err_col="dY", style="scatter")
        yk_res = app.fit_york(graph=g_yk.name, table=t_yk.name, x_err_col="dX", y_err_col="dY")
        assert yk_res.get("converged") is True and abs(yk_res["slope"] - 2.4) < 0.15 and yk_res["slope_se"] > 0

        # Circle Implicit ODR fit: (x - 3.5)^2 + (y + 2.0)^2 = 6.25^2
        xc_t, yc_t, r_t = 3.5, -2.0, 6.25
        rows_cir = [
            [xc_t + r_t * math.cos(2.0 * math.pi * k / 36.0), yc_t + r_t * math.sin(2.0 * math.pi * k / 36.0)]
            for k in range(36)
        ]
        t_cir = app.create_table(rows=len(rows_cir), cols=2, name="CircleOdrTbl")
        populate_table_rows(t_cir, rows_cir, ["X", "Y"])
        g_cir = app.plot_xy(t_cir, x_col=0, y_cols=[1], style="scatter")
        odr_res = app.fit_implicit_odr(graph=g_cir.name, shape="circle")
        assert odr_res.get("converged") is True, f"Circle ODR failed: {odr_res}"
        cp = odr_res["params"]
        assert (
            "xc" in cp
            and abs(cp["xc"] - xc_t) < 0.02
            and abs(cp["yc"] - yc_t) < 0.02
            and abs(cp["R"] - r_t) < 0.02
        ), f"Circle ODR wrong params: {odr_res}"

    runner.check(
        "3.2 Global Fit, Dataset F-Test, Piecewise, Huber/Bisquare Robust, York & Circle ODR",
        _global_piecewise_robust_york_odr,
    )

    def _logistic_mlr_model_compare() -> None:
        # Logistic regression
        rows_lr = []
        for i in range(60):
            x = -3.0 + 6.0 * (i / 59.0)
            p = 1.0 / (1.0 + math.exp(-(-0.8 + 2.2 * x)))
            y = 1.0 if p >= 0.5 else 0.0
            if i in (27, 32):  # slight overlap near boundary so IRLS stays finite
                y = 1.0 - y
            rows_lr.append([x, y])
        t_lr = app.create_table(rows=len(rows_lr), cols=2, name="LogisticTbl")
        populate_table_rows(t_lr, rows_lr, ["Dose", "Response"])
        lr_res = app.fit_logistic(table=t_lr.name, y_col="Response", x_cols=["Dose"], plot=True)
        assert lr_res.get("converged") is True and lr_res.get("auc", 0.0) > 0.92 and lr_res["betas"][1] > 0.5

        # Multiple Linear Regression (MLR): y = 5.0 + 2.5*x1 - 1.75*x2
        rng = random.Random(101)
        rows_mlr = []
        for i in range(35):
            x1 = float(i % 7) + 0.2 * (i // 7)
            x2 = float((i * 3) % 11) - 0.1 * i
            y = 5.0 + 2.5 * x1 - 1.75 * x2 + rng.gauss(0.0, 0.01)
            rows_mlr.append([x1, x2, y])
        t_mlr = app.create_table(rows=len(rows_mlr), cols=3, name="MlrTbl")
        populate_table_rows(t_mlr, rows_mlr, ["X1", "X2", "Y"])
        mlr_res = app.fit_mlr(table=t_mlr.name, y_col="Y", x_cols=["X1", "X2"], plot_diagnostics=True)
        assert mlr_res.get("ok") is True and mlr_res.get("r2", 0.0) > 0.999
        b = mlr_res["betas"]
        assert abs(b[0] - 5.0) < 0.05 and abs(b[1] - 2.5) < 0.05 and abs(b[2] - (-1.75)) < 0.05
        assert len(mlr_res.get("vif", [])) == 2

        # AIC / F-test Model Comparison: bi-exponential data -> ExpDec2 beats ExpDec1
        rows_cmp = [
            [float(t), 3.0 + 90.0 * math.exp(-t / 4.0) + 45.0 * math.exp(-t / 35.0)]
            for t in range(0, 121, 2)
        ]
        t_cmp = app.create_table(rows=len(rows_cmp), cols=2, name="ModelCmpTbl")
        populate_table_rows(t_cmp, rows_cmp, ["t", "Signal"])
        g_cmp = app.plot_xy(t_cmp, x_col=0, y_cols=[1], style="line")
        cmp_res = app.compare_models(graph=g_cmp.name, models=["ExpDec1", "ExpDec2"])
        assert cmp_res.get("ok") is True and cmp_res.get("best_model") == "ExpDec2", (
            f"Expected ExpDec2 to win AIC comparison, got {cmp_res}"
        )

    runner.check("3.3 Logistic Regression, MLR (with VIF) & AIC/F-Test Model Comparison", _logistic_mlr_model_compare)


def run_workflow_4_materials_science(runner: E2ERunner, app: tg.Session) -> None:
    def _matsci() -> None:
        # 4A. Arrhenius hydrogen diffusivity D(T) = D0 * exp(-Ea / (kB * T)), Ea = 0.420 eV
        kb_ev = 8.617333262e-5
        ea_true = 0.420
        d0_true = 2.4e-7
        rows_arr = [
            [t_k, d0_true * math.exp(-ea_true / (kb_ev * t_k))]
            for t_k in (300.0, 330.0, 360.0, 400.0, 450.0, 500.0, 550.0)
        ]
        t_arr = app.create_table(rows=len(rows_arr), cols=2, name="ArrheniusDiffTbl")
        populate_table_rows(t_arr, rows_arr, ["TempK", "Diffusivity"])
        arr_res = app.arrhenius(table=t_arr.name, temp_col="TempK", rate_col="Diffusivity", celsius=False)
        assert arr_res.get("ok") is True, f"Arrhenius failed: {arr_res}"
        assert abs(arr_res["ea_ev"] - ea_true) < 0.01 and arr_res["r2"] > 0.9999, (
            f"Arrhenius Ea={arr_res.get('ea_ev')} != {ea_true}"
        )
        assert_graph_arial(app, arr_res["graph"], "Arrhenius Plot")

        # 4B. Tauc direct bandgap (alpha * hv)^2 = A^2 * (hv - Eg), Eg = 3.25 eV
        eg_true = 3.25
        rows_tauc = []
        for i in range(65):
            hv = 2.60 + 1.50 * (i / 64.0)  # 2.60 .. 4.10 eV
            if hv <= eg_true:
                alpha = 15.0 * math.exp((hv - eg_true) / 0.06)  # small sub-gap Urbach tail
            else:
                alpha = math.sqrt(2.5e9 * (hv - eg_true)) / hv
            rows_tauc.append([hv, alpha])
        t_tauc = app.create_table(rows=len(rows_tauc), cols=2, name="TaucZnOTbl")
        populate_table_rows(t_tauc, rows_tauc, ["Energy_eV", "Alpha"])
        tauc_res = app.tauc(
            table=t_tauc.name,
            x_col="Energy_eV",
            alpha_col="Alpha",
            wavelength_nm=False,
            energy_ev=True,
            direct=True,
        )
        assert tauc_res.get("ok") is True, f"Tauc failed: {tauc_res}"
        assert abs(tauc_res["eg_ev"] - eg_true) < 0.03, f"Tauc Eg={tauc_res.get('eg_ev')} != {eg_true}"
        assert_graph_arial(app, tauc_res["graph"], "Tauc Plot")

        # 4C. EIS Randles + CPE Nyquist semicircle: Z(w) = Rs + Rct / (1 + Rct * Q * (j*w)^n)
        rs_true, rct_true, q_true, n_true = 25.0, 180.0, 1.0e-4, 0.92
        rows_eis = []
        for k in range(55):
            freq = 10.0 ** (-2.0 + 7.0 * (k / 54.0))
            omega = 2.0 * math.pi * freq
            # (j * omega)^n = omega^n * (cos(n*pi/2) + j*sin(n*pi/2))
            wn = omega ** n_true
            ang = 0.5 * math.pi * n_true
            denom_re = 1.0 + rct_true * q_true * wn * math.cos(ang)
            denom_im = rct_true * q_true * wn * math.sin(ang)
            d2 = denom_re * denom_re + denom_im * denom_im
            z_re = rs_true + rct_true * denom_re / d2
            z_im = -rct_true * denom_im / d2
            rows_eis.append([freq, z_re, z_im])
        t_eis = app.create_table(rows=len(rows_eis), cols=3, name="EISRandlesTbl")
        populate_table_rows(t_eis, rows_eis, ["FreqHz", "Zre", "Zim"])
        eis_res = app.eis_fit(
            table=t_eis.name,
            freq_col="FreqHz",
            z_real_col="Zre",
            z_imag_col="Zim",
            z_imag_negated=False,
            fit_cpe=True,
        )
        assert eis_res.get("ok") is True, f"EIS fit failed: {eis_res}"
        assert abs(eis_res["rs"] - rs_true) < 1.0, f"EIS Rs={eis_res['rs']} != {rs_true}"
        assert abs(eis_res["rct"] - rct_true) < 2.0, f"EIS Rct={eis_res['rct']} != {rct_true}"
        assert abs(eis_res["n_cpe"] - n_true) < 0.02, f"EIS n={eis_res['n_cpe']} != {n_true}"
        assert_graph_arial(app, eis_res["nyquist_graph"], "EIS Nyquist")
        print(
            f"  -> MatSci: Ea={arr_res['ea_ev']:.4f} eV, Eg={tauc_res['eg_ev']:.3f} eV, "
            f"EIS(Rs={eis_res['rs']:.2f}, Rct={eis_res['rct']:.2f}, n={eis_res['n_cpe']:.3f})"
        )

    runner.check("4. Materials Science Workflows (Arrhenius Ea, Tauc Direct Eg, EIS Randles+CPE)", _matsci)


def run_workflow_5_signal_and_matrix_3d(
    runner: E2ERunner, app: tg.Session
) -> Dict[str, str]:
    refs: Dict[str, str] = {}

    def _signal_1d() -> None:
        rng = random.Random(505)
        fs = 500.0
        n = 512
        rows = []
        for i in range(n):
            t = i / fs
            env_amp = 1.0 + 0.5 * math.cos(2.0 * math.pi * 2.0 * t)
            sig = env_amp * math.sin(2.0 * math.pi * 15.0 * t) + 0.6 * math.sin(2.0 * math.pi * 120.0 * t)
            rows.append([t, sig + rng.gauss(0.0, 0.08)])
        t_sig = app.create_table(rows=n, cols=2, name="ChirpTwoToneTbl")
        populate_table_rows(t_sig, rows, ["Time", "Signal"])
        g_sig = app.plot_xy(t_sig, x_col=0, y_cols=[1], style="line", graph_name="Signal1DGraph")
        raw_curve = g_sig.info()["layers"][0]["curves"][0]["title"]

        assert app.smooth(graph=g_sig.name, curve=raw_curve, method="savgol", window=9, poly_order=3)["ok"]
        assert app.smooth(graph=g_sig.name, curve=raw_curve, method="whittaker", **{"lambda": 50.0})["ok"]
        assert app.smooth(graph=g_sig.name, curve=raw_curve, method="lowess", span=0.08)["ok"]
        assert app.savgol_derivative(graph=g_sig.name, curve=raw_curve, deriv_order=1, window=9, poly_order=3)["ok"]
        assert app.iir_filter(
            graph=g_sig.name,
            curve=raw_curve,
            design="butterworth",
            pass_type="lowpass",
            order=4,
            cutoff=40.0,
            zero_phase=True,
        )["ok"]
        fft_res = app.fft(table=t_sig.name, y_col="Signal")
        assert fft_res.get("ok") is True and "table" in fft_res
        psd_res = app.welch_psd(graph=g_sig.name, curve=raw_curve, segment_len=128, plot=True)
        assert psd_res.get("ok") is True and "table" in psd_res
        stft_res = app.stft(graph=g_sig.name, curve=raw_curve, window_size=64, hop_size=16, plot=True)
        assert stft_res.get("ok") is True and stft_res.get("rows", 0) > 4
        env_res = app.envelope(graph=g_sig.name, curve=raw_curve, method="hilbert")
        assert env_res.get("ok") is True
        cwt_res = app.wavelet_cwt(graph=g_sig.name, curve=raw_curve, scales=24, wavelet="morlet", plot=True)
        assert cwt_res.get("ok") is True
        dwt_res = app.wavelet_denoise(graph=g_sig.name, curve=raw_curve, wavelet="db4", levels=3)
        assert dwt_res.get("ok") is True

        # Fourier Self-Deconvolution (FSD) on overlapping Lorentzian doublet
        rows_fsd = []
        for i in range(256):
            x = -25.0 + 50.0 * (i / 255.0)
            y = 1.0 / (1.0 + ((x + 3.0) / 3.2) ** 2) + 1.0 / (1.0 + ((x - 3.0) / 3.2) ** 2)
            rows_fsd.append([x, y])
        t_fsd = app.create_table(rows=len(rows_fsd), cols=2, name="FsdDoubletTbl")
        populate_table_rows(t_fsd, rows_fsd, ["WaveNum", "Absorbance"])
        g_fsd = app.plot_xy(t_fsd, x_col=0, y_cols=[1], style="line")
        fsd_res = app.fsd(graph=g_fsd.name, fwhm=5.0, k=1.8)
        assert fsd_res.get("ok") is True

    runner.check(
        "5.1 1D Signal Processing (SavGol/Whittaker/LOWESS, Deriv, Butterworth, FFT, Welch, STFT, Hilbert, CWT/DWT, FSD)",
        _signal_1d,
    )

    def _matrix_2d_3d() -> None:
        rows_n, cols_n = 41, 41
        x_min, x_max, y_min, y_max = -4.0, 4.0, -4.0, 4.0
        m_raw = app.create_matrix(
            rows=rows_n, cols=cols_n, name="CT_Strain_Raw", x_min=x_min, x_max=x_max, y_min=y_min, y_max=y_max
        )
        grid: List[List[float]] = []
        for r in range(rows_n):
            yv = y_min + (y_max - y_min) * (r / (rows_n - 1.0))
            row_vals: List[float] = []
            for c in range(cols_n):
                xv = x_min + (x_max - x_min) * (c / (cols_n - 1.0))
                gauss = 100.0 * math.exp(-(((xv - 0.4) ** 2) / (2.0 * 0.9**2) + ((yv + 0.3) ** 2) / (2.0 * 1.1**2)))
                tilt = 10.0 + 2.5 * xv - 1.8 * yv
                row_vals.append(gauss + tilt)
            grid.append(row_vals)
        grid[5][5] += 950.0  # Cosmic-ray spike!
        m_raw.set_data(grid)

        m_detilt_info = app.call("matrix.filter2d", matrix=m_raw.name, method="planar_tilt")
        m_clean_info = app.call("matrix.filter2d", matrix=m_detilt_info["name"], method="median", radius=1)
        m_clean = app.matrix(m_clean_info["name"])
        clean_data = m_clean.get_data()
        assert clean_data[5][5] < 20.0, f"Median 2D filter failed to remove cosmic-ray spike: {clean_data[5][5]}"
        assert max(max(rw) for rw in clean_data) > 80.0, "Central 2D Gaussian peak attenuated too much"

        fft2_info = app.call("matrix.fft2d", matrix=m_clean.name, inverse=False, shift_center=True)
        assert fft2_info.get("ok") is True

        pk2d = m_clean.analyze_2d_peaks(max_peaks=3, prominence=0.2)
        assert pk2d.get("ok") is True and pk2d.get("peak_count", 0) >= 1

        g_hm = app.plot_matrix(m_clean, style="colormap")
        g_hm.set_colorbar(palette="viridis", visible=True, title="Strain (%)")
        assert_graph_arial(app, g_hm.name, "Matrix Viridis Heatmap + Colorbar")

        g_cnt = app.plot_matrix(m_clean, style="contour")
        assert_graph_arial(app, g_cnt.name, "Matrix Contour")

        g_3d = app.plot_3d_matrix(m_clean, style="surface")
        info_3d = app.window_info(g_3d.name)
        assert info_3d.get("type") == "Graph3D"

        sfit = app.fit_surface_3d(matrix=m_clean.name, model="gauss2d")
        assert sfit.get("converged") is True and sfit.get("r2", 0.0) > 0.97, f"3D Gauss2D surface fit failed: {sfit}"

        refs["clean_matrix"] = m_clean.name
        refs["heatmap_graph"] = g_hm.name

    runner.check(
        "5.2 2D Matrix Detilt + Despike, 2D FFT, 2D Peaks, Viridis Colorbar Heatmap, Contour, 3D Surface & 3D Fit",
        _matrix_2d_3d,
    )
    return refs


def run_workflow_6_stats_gadgets_curvemath_digitizer_project_updater(
    runner: E2ERunner,
    app: tg.Session,
    tmp_dir: Path,
    saved_refs: Dict[str, str],
) -> None:
    def _stats_suite() -> None:
        rng = random.Random(2026)

        # Normality (Shapiro-Wilk) + Outliers (Grubbs & GESD)
        gauss_vals = [[100.0 + rng.gauss(0.0, 2.0)] for _ in range(32)]
        t_norm = app.create_table(rows=32, cols=1, name="NormalityTbl")
        populate_table_rows(t_norm, gauss_vals, ["Strength"])
        nr = app.normality(table=t_norm.name, col="Strength")
        assert nr["ok"] and nr["shapiro_wilk_w"] > 0.90 and nr["shapiro_wilk_p"] > 0.05

        out_vals = [[50.0 + 0.4 * math.sin(i * 1.1)] for i in range(28)] + [[145.0], [142.0]]
        t_out = app.create_table(rows=30, cols=1, name="OutlierTbl")
        populate_table_rows(t_out, out_vals, ["Val"])
        gesd = app.outlier(table=t_out.name, col="Val", method="gesd", max_outliers=4)
        assert gesd["ok"] and 28 in gesd["outlier_rows"] and 29 in gesd["outlier_rows"]

        t_gr = app.create_table(rows=29, cols=1, name="GrubbsTbl")
        populate_table_rows(t_gr, out_vals[:29], ["Val"])
        grubbs = app.outlier(table=t_gr.name, col="Val", method="grubbs")
        assert grubbs["ok"] and 28 in grubbs["outlier_rows"] and grubbs["p_value"] < 0.01

        # One-Way ANOVA + Tukey, Kruskal-Wallis & Mann-Whitney
        grp_rows = [
            [10.0 + rng.gauss(0, 0.7), 15.0 + rng.gauss(0, 0.7), 22.0 + rng.gauss(0, 0.7)]
            for _ in range(18)
        ]
        t_aov = app.create_table(rows=18, cols=3, name="AnovaGroupsTbl")
        populate_table_rows(t_aov, grp_rows, ["G1", "G2", "G3"])
        aov1 = app.anova_oneway(table=t_aov.name, cols=["G1", "G2", "G3"])
        assert aov1["ok"] and aov1["p_value"] < 1e-8

        kw = app.nonparametric(table=t_aov.name, cols=["G1", "G2", "G3"], method="kruskal_wallis")
        mw_res = app.nonparametric(table=t_aov.name, cols=["G1", "G2"], method="mann_whitney")
        assert kw["ok"] and kw["p_value"] < 1e-5 and mw_res["ok"] and mw_res["p_value"] < 1e-4

        # Two-Way ANOVA
        tw_rows = []
        for a in (0.0, 1.0):
            for b in (0.0, 1.0, 2.0):
                for rep in range(6):
                    tw_rows.append([a, b, 20.0 + 8.0 * a + 4.0 * b + 0.15 * (rep - 2.5)])
        t_tw = app.create_table(rows=len(tw_rows), cols=3, name="TwoWayAnovaTbl")
        populate_table_rows(t_tw, tw_rows, ["Alloy", "Temper", "HV"])
        aov2 = app.anova_twoway(table=t_tw.name, factor_a="Alloy", factor_b="Temper", response="HV")
        assert aov2["ok"] and aov2["p_factor_a"] < 1e-6 and aov2["p_factor_b"] < 1e-6

        # Bland-Altman & Passing-Bablok (with gross outlier invariance)
        ba_rows = [[float(10 + i * 3), float(10 + i * 3) - 2.5 + 0.2 * math.sin(i)] for i in range(25)]
        t_ba = app.create_table(rows=len(ba_rows), cols=2, name="BlandAltmanTbl")
        populate_table_rows(t_ba, ba_rows, ["MethodA", "MethodB"])
        ba = app.bland_altman(table=t_ba.name, col_a="MethodA", col_b="MethodB")
        assert ba["ok"] and abs(abs(ba["mean_bias"]) - 2.5) < 0.2

        pb_rows = [[float(5 + i * 4), 1.25 * float(5 + i * 4) + 4.0] for i in range(26)]
        pb_rows[13][1] = 6000.0  # gross outlier
        t_pb = app.create_table(rows=len(pb_rows), cols=2, name="PassingBablokTbl")
        populate_table_rows(t_pb, pb_rows, ["Ref", "Test"])
        pb = app.passing_bablok(table=t_pb.name, x_col="Ref", y_col="Test")
        assert pb["ok"] and abs(pb["slope"] - 1.25) < 0.05 and abs(pb["intercept"] - 4.0) < 0.5

        # 3-Parameter Weibull (beta=3.5, eta=200.0, gamma=120.0) with censored suspensions
        wb_rows = []
        for r in range(36):
            p = (r + 1.0 - 0.3) / (36.0 + 0.4)
            x_val = 120.0 + 200.0 * ((-math.log(1.0 - p)) ** (1.0 / 3.5))
            event_flag = 0.0 if (r % 6 == 5) else 1.0  # 1=Failure, 0=Censored suspension
            wb_rows.append([x_val, event_flag])
        t_wb = app.create_table(rows=len(wb_rows), cols=2, name="Weibull3PTbl")
        populate_table_rows(t_wb, wb_rows, ["Cycles", "Event"])
        wb = app.weibull(
            table=t_wb.name,
            col="Cycles",
            censor_col="Event",
            censor_coding=0,
            three_param=True,
        )
        assert wb["ok"] and wb["three_param"] is True and wb["censored_count"] == 6, f"Weibull 3P failed: {wb}"
        assert abs(wb["gamma"] - 120.0) < 25.0 and wb["beta"] > 2.0 and wb["r2"] > 0.97

        # Kaplan-Meier + log-rank + ROC AUC
        km_rows = (
            [[float(2 + i * 2), 1.0 if i % 4 != 3 else 0.0, 0.0] for i in range(16)]
            + [[float(15 + i * 5), 1.0 if i % 3 != 2 else 0.0, 1.0] for i in range(16)]
        )
        t_km = app.create_table(rows=len(km_rows), cols=3, name="KaplanMeierTbl")
        populate_table_rows(t_km, km_rows, ["Time", "Event", "Group"])
        km = app.kaplan_meier(table=t_km.name, time_col="Time", event_col="Event", group_col="Group")
        assert km["ok"] and km["log_rank_p"] < 0.01

        roc_rows = [[0.0, 0.1 + 0.02 * i] for i in range(20)] + [[1.0, 0.55 + 0.02 * i] for i in range(20)]
        t_roc = app.create_table(rows=len(roc_rows), cols=2, name="RocTbl")
        populate_table_rows(t_roc, roc_rows, ["State", "Score"])
        roc_res = app.roc(table=t_roc.name, state_col="State", score_col="Score")
        assert roc_res["ok"] and roc_res["auc"] > 0.98

        # Multivariate: PCA Biplot, Correlation Matrix & K-Means Clustering
        mv_rows = []
        centers = [(0.0, 0.0, 0.0), (10.0, 10.5, -9.5), (-10.0, -9.5, 10.0)]
        for ci, (cx, cy, cz) in enumerate(centers):
            for j in range(12):
                d = 0.3 * (j - 5.5)
                mv_rows.append([cx + d, cy + 0.95 * d + 0.05 * ((j % 3) - 1), cz - 0.9 * d + 0.07 * (j % 2)])
        t_mv = app.create_table(rows=len(mv_rows), cols=3, name="MultivarTbl")
        populate_table_rows(t_mv, mv_rows, ["F1", "F2", "F3"])
        pca_res = app.pca(table=t_mv.name, cols=["F1", "F2", "F3"])
        assert pca_res["ok"] and "scores_table" in pca_res
        corr_res = app.correlation(table=t_mv.name, cols=["F1", "F2", "F3"])
        assert corr_res["ok"] and corr_res["r_matrix"][0][1] > 0.95 and corr_res["r_matrix"][0][2] < -0.95
        clus_res = app.clustering(table=t_mv.name, cols=["F1", "F2", "F3"], k=3)
        assert clus_res["ok"] and clus_res["silhouette"] > 0.85

    runner.check(
        "6.1 Statistics, 3-Param Censored Weibull, Kaplan-Meier, ROC, PCA, Correlation & K-Means",
        _stats_suite,
    )

    def _gadgets_curvemath_digitizer() -> None:
        # Gadgets (roi_stats, integrate, delta_cursor) on y = 4*x + 2
        t_gd = app.create_table(rows=21, cols=2, name="GadgetCurveTbl")
        populate_table_rows(t_gd, [[float(x), 4.0 * float(x) + 2.0] for x in range(21)], ["X", "Y"])
        g_gd = app.plot_xy(t_gd, x_col=0, y_cols=[1], style="line_symbol")

        r1 = app.attach_gadget(graph=g_gd.name, type="roi_stats", x_min=2.0, x_max=8.0, y_min=-100.0, y_max=200.0)
        assert r1["ok"]
        r1_moved = app.set_gadget_roi(graph=g_gd.name, x_min=5.0, x_max=15.0, y_min=-100.0, y_max=200.0)
        assert r1_moved["ok"] and abs(r1_moved["primary_metric"] - 42.0) < 1e-6  # mean of 4x+2 on [5..15] is 42

        r_int = app.attach_gadget(graph=g_gd.name, type="integrate", x_min=0.0, x_max=10.0, y_min=-10.0, y_max=200.0)
        assert r_int["ok"] and abs(r_int["primary_metric"] - 220.0) < 1e-4  # integral_0^10 (4x+2) dx = 220

        r_dc = app.attach_gadget(graph=g_gd.name, type="delta_cursor", x1=3.0, x2=9.0)
        q_dc = app.query_gadget(graph=g_gd.name)
        assert q_dc["ok"] and abs(q_dc["dx"] - 6.0) < 1e-6 and abs(q_dc["dy"] - 24.0) < 1e-6 and abs(q_dc["slope"] - 4.0) < 1e-6
        assert app.clear_gadgets(graph=g_gd.name)["ok"]

        # Curve Math (subtract, average_curves, normalize, intersections, polygon_area, fill_between)
        t_cm = app.create_table(rows=21, cols=3, name="CurveMathTbl")
        populate_table_rows(
            t_cm,
            [[float(x), 2.0 * x, 20.0 - 2.0 * x] for x in range(21)],
            ["X", "CA", "CB"],
        )
        g_cm = app.plot_xy(t_cm, x_col=0, y_cols=[1, 2], style="line")
        curves = [c["title"] for c in g_cm.info()["layers"][0]["curves"]]
        c_a, c_b = curves[0], curves[1]

        ix = app.intersections(graph=g_cm.name, curve_a=c_a, curve_b=c_b)
        assert ix["ok"] and ix["count"] >= 1 and abs(ix["points"][0]["x"] - 5.0) < 1e-6 and abs(ix["points"][0]["y"] - 10.0) < 1e-6
        assert app.fill_between(graph=g_cm.name, curve_a=c_a, curve_b=c_b)["ok"]
        assert app.curve_eval(graph=g_cm.name, curve_a=c_a, curve_b=c_b, op="-")["ok"]
        assert app.average_curves(graph=g_cm.name, curves=[c_a, c_b])["ok"]
        assert app.normalize_curve(graph=g_cm.name, curve=c_a, mode="0_1")["ok"]

        t_loop = app.create_table(rows=5, cols=2, name="PolygonLoopTbl")
        populate_table_rows(
            t_loop,
            [[0.0, 0.0], [10.0, 0.0], [10.0, 6.0], [0.0, 6.0], [0.0, 0.0]],
            ["X", "Y"],
        )
        g_loop = app.plot_xy(t_loop, x_col=0, y_cols=[1], style="line")
        poly = app.polygon_area(graph=g_loop.name)
        assert poly["ok"] and abs(poly["area"] - 60.0) < 1e-6 and abs(poly["arc_length"] - 32.0) < 1e-6

        # Digitizer: synthetic red line y = 0.5*x + 15.0 on calibrated 500x400 PNG
        dig_png = tmp_dir / "synthetic_digitizer_plot.png"
        write_synthetic_digitizer_png(dig_png)
        dig = app.digitize(
            dig_png,
            px_x1=60,
            py_x1=350,
            val_x1=0.0,
            px_x2=460,
            py_x2=350,
            val_x2=100.0,
            px_y1=60,
            py_y1=350,
            val_y1=0.0,
            px_y2=60,
            py_y2=50,
            val_y2=100.0,
            color="#FF0000",
            tolerance=45,
            samples=60,
            plot=True,
        )
        assert dig.get("ok") is True and dig.get("count", 0) >= 40, f"Digitizer failed: {dig}"
        xs_d, ys_d = dig["x"], dig["y"]
        max_y_err = max(
            abs(yv - (0.5 * xv + 15.0))
            for xv, yv in zip(xs_d, ys_d)
            if 12.0 <= xv <= 88.0
        )
        assert max_y_err < 0.8, f"Digitizer extracted Y deviated by {max_y_err:.3f} from ground truth"

    runner.check("6.2 Gadgets (ROI/Integrate/DeltaCursor), Curve Math & Graph Digitizer", _gadgets_curvemath_digitizer)

    def _sciprj_roundtrip_and_mock_updater() -> None:
        # .sciprj project save -> new_project -> open_project round-trip
        sciprj_path = tmp_dir / "full_e2e_roundtrip.sciprj"
        app.save_project(sciprj_path)
        assert sciprj_path.is_file() and sciprj_path.stat().st_size > 4000

        app.new_project()
        assert len(app.list_windows()) == 0, "new_project() did not clear windows"

        app.open_project(sciprj_path)
        win_names = {w["name"] for w in app.list_windows()}
        for expected_key in ("tensile_graph", "tensile_table", "clean_matrix", "heatmap_graph"):
            expected_name = saved_refs.get(expected_key)
            if expected_name:
                assert expected_name in win_names, (
                    f"Window '{expected_name}' lost across .sciprj round-trip! Present: {sorted(win_names)}"
                )
        for underscore_win in ("AlZnMg_TDS", "TwoWayAnovaTbl_InteractionCells"):
            assert underscore_win in win_names, (
                f"Underscore-named table '{underscore_win}' missing after .sciprj reload!"
            )
        if "TDS-Deconv-Graph" in win_names:
            tds_reloaded = assert_graph_arial(app, "TDS-Deconv-Graph", ".sciprj Reloaded TDS Graph")
            assert tds_reloaded["layers"][0]["curve_count"] >= 5, (
                f"TDS graph lost curves across .sciprj reload: {tds_reloaded['layers'][0]}"
            )
        if "tensile_graph" in saved_refs:
            reloaded_info = assert_graph_arial(app, saved_refs["tensile_graph"], ".sciprj Reloaded ISIJ Graph")
            assert abs(reloaded_info.get("page_width", 0.0) - 85.0) < 0.5

        # Local HTTP mock GitHub Release feed for app.check_updates
        payload_bytes = b"PK\x03\x04MockToriGraphUpdateArchive"
        sha_hex = hashlib.sha256(payload_bytes).hexdigest()

        class _MockReleaseHandler(http.server.BaseHTTPRequestHandler):
            def do_GET(self) -> None:  # noqa: N802
                body = json.dumps(
                    {
                        "tag_name": "v2.5.0",
                        "name": "ToriGraph 2.5.0 Mock Release",
                        "draft": False,
                        "prerelease": False,
                        "published_at": "2026-11-12T10:00:00Z",
                        "html_url": "https://github.com/kzykshmz/ToriGraph/releases/tag/v2.5.0",
                        "body": "Mock release notes",
                        "assets": [
                            {
                                "name": "ToriGraph-2.5.0-win64.zip",
                                "size": len(payload_bytes),
                                "digest": f"sha256:{sha_hex}",
                                "browser_download_url": "http://127.0.0.1/ToriGraph-2.5.0-win64.zip",
                            }
                        ],
                    }
                ).encode("utf-8")
                self.send_response(200)
                self.send_header("Content-Type", "application/json; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def log_message(self, format: str, *args: Any) -> None:  # noqa: A002
                pass

        httpd = http.server.HTTPServer(("127.0.0.1", 0), _MockReleaseHandler)
        port = httpd.server_address[1]
        srv_thread = threading.Thread(target=httpd.serve_forever, daemon=True)
        srv_thread.start()
        try:
            upd = app.check_updates(
                feed_url=f"http://127.0.0.1:{port}/releases/latest",
                current_version="2.4.0",
            )
            assert upd.get("ok") is True and upd.get("update_available") is True, f"check_updates failed: {upd}"
            assert upd.get("latest_version") == "2.5.0" and upd.get("sha256") == sha_hex
        finally:
            httpd.shutdown()
            httpd.server_close()

    runner.check("6.3 .sciprj Full Project Round-Trip & Local HTTP Mock Auto-Updater", _sciprj_roundtrip_and_mock_updater)


def run_workflow_7_cli_run_python(runner: E2ERunner, tmp_dir: Path) -> None:
    def _cli_headless() -> None:
        exe_candidates = [
            REPO_ROOT / "build" / "scidavis" / "Release" / "ToriGraph.exe",
            REPO_ROOT / "build" / "scidavis" / "ToriGraph.exe",
        ]
        exe = next((p for p in exe_candidates if p.is_file()), None)
        assert exe is not None, f"ToriGraph.exe not found in {exe_candidates}"

        out_json = tmp_dir / "cli_run_python_result.json"
        out_png = tmp_dir / "cli_run_python_plot.png"
        script_py = tmp_dir / "cli_worker_script.py"
        script_py.write_text(
            f"""import json
import torigraph as tg

app = tg.connect()
t = app.create_table(rows=6, cols=2, name="CliTbl")
t.set_data_dict({{"X": [float(i) for i in range(6)], "Y": [3.5 * float(i) + 1.25 for i in range(6)]}})
g = app.plot_xy(t, x_col=0, y_cols=[1], style="line_symbol")
g.apply_style_preset("isij_85mm")
fit = app.fit_linear(graph=g.name)
g.export({str(out_png)!r}, dpi=300)
with open({str(out_json)!r}, "w", encoding="utf-8") as fh:
    json.dump({{"slope": fit["slope"], "intercept": fit["intercept"], "r2": fit["r2"]}}, fh)
""",
            encoding="utf-8",
        )

        env = os.environ.copy()
        deps_bin = REPO_ROOT / ".deps" / "Library" / "bin"
        env["PATH"] = f"{exe.parent}{os.pathsep}{deps_bin}{os.pathsep}{env.get('PATH', '')}"
        env["TORIGRAPH_PYTHON_EXECUTABLE"] = sys.executable

        proc = subprocess.run(
            [str(exe), "--headless", "--run-python", str(script_py)],
            cwd=str(exe.parent),
            env=env,
            capture_output=True,
            text=True,
            timeout=60,
        )
        assert proc.returncode == 0, (
            f"CLI --run-python exited with code {proc.returncode}!\nstdout:\n{proc.stdout}\nstderr:\n{proc.stderr}"
        )
        assert out_json.is_file() and out_png.is_file() and out_png.stat().st_size > 2000
        res = json.loads(out_json.read_text(encoding="utf-8"))
        assert abs(res["slope"] - 3.5) < 1e-9 and abs(res["intercept"] - 1.25) < 1e-9 and abs(res["r2"] - 1.0) < 1e-9

    runner.check("7. CLI `--headless --run-python <script.py>` End-to-End Execution", _cli_headless)


def run_workflow_8_round2_critic_regressions(
    runner: E2ERunner, app: tg.Session, tmp_dir: Path
) -> None:
    def _small_sample_and_strict_validation() -> None:
        app.new_project()

        # #137 P0-1 & P0-2: 2-row smooth(method="average", points=7) & 1-row smooth/fft/fit
        t2 = app.create_table(rows=2, col_names=["X", "Y"], designations=["X", "Y"], name="TwoRowAdvTbl")
        t2.set_data_dict({"X": [1.0, 2.0], "Y": [10.0, 20.0]})
        t0 = time.monotonic()
        try:
            app.smooth(method="average", table=t2.name, x_col="X", y_col="Y", points=7)
        except tg.RpcError as exc:
            assert exc.code == -32602, f"Expected -32602 for 2-row window=7 smooth, got {exc}"
        assert (time.monotonic() - t0) < 3.0, "2-row smooth hung instead of returning promptly!"
        assert app.ping().get("pong") is True, "Server died on 2-row average smooth!"

        t1 = app.create_table(rows=1, col_names=["X", "Y"], designations=["X", "Y"], name="OneRowAdvTbl")
        t1.set_data_dict({"X": [5.0], "Y": [42.0]})
        for fn in (
            lambda: app.smooth(method="savgol", table=t1.name, x_col="X", y_col="Y", window=5, poly_order=2),
            lambda: app.fft(table=t1.name, x_col="X", y_col="Y"),
            lambda: app.fit_linear(table=t1.name, x_col="X", y_col="Y"),
        ):
            t_start = time.monotonic()
            raised_clean = False
            try:
                fn()
            except tg.RpcError as exc:
                assert exc.code == -32602, f"Expected -32602 on 1-row degenerate input, got {exc}"
                raised_clean = True
            assert raised_clean, "1-row degenerate operation should raise -32602 RpcError"
            assert (time.monotonic() - t_start) < 3.0, "1-row operation hung on modal dialog!"
        assert app.ping().get("pong") is True

        # #137 P2-1: Strict integer & bounds validation (-32602)
        g_val = app.plot_xy(t2, x_col="X", y_cols=["Y"], graph_name="ValCheckGraph")
        for bad_call in (
            lambda: app.call("table.create", rows="abc", cols=2),
            lambda: app.call("table.create", rows=5, cols=-3),
            lambda: app.call("matrix.create", rows=0, cols=4),
            lambda: g_val.set_curve_style(curve_index=-5, color="#FF0000"),
            lambda: g_val.set_curve_style(curve_index=999, color="#FF0000"),
        ):
            got_32602 = False
            try:
                bad_call()
            except tg.RpcError as exc:
                assert exc.code == -32602, f"Expected -32602 for invalid params, got {exc}"
                got_32602 = True
            assert got_32602, "Invalid parameter call failed to raise RpcError(-32602)"

    runner.check(
        "8.1 Hostile 1-Row/2-Row Signal/Fit Crash-Modal Guards & Strict -32602 Validation",
        _small_sample_and_strict_validation,
    )

    def _unicode_gzip_sciprj_corrupt_append_and_hidden() -> None:
        app.new_project()

        # #137 P2-2: create_table(col_names, designations), get_data_dict/to_dict, clone_window, list_actions
        vis_tbl = app.create_table(
            rows=8,
            name="VisUnicodeTbl",
            col_names=["Time", "SignalA", "SignalB"],
            designations=["X", "Y", "Y"],
        )
        xs = [float(i) for i in range(8)]
        ya = [2.0 * x + 1.5 for x in xs]
        yb = [math.exp(-0.2 * (x - 3.0) ** 2) * 10.0 + 1.0 for x in xs]
        vis_tbl.set_data_dict({"Time": xs, "SignalA": ya, "SignalB": yb})

        dd = vis_tbl.get_data_dict()
        dd_alias = vis_tbl.get_data_dict(["Time", "SignalB"])
        assert list(dd.keys()) == ["Time", "SignalA", "SignalB"]
        assert len(dd["SignalA"]) == 8 and abs(dd["SignalA"][3] - 7.5) < 1e-9
        assert list(dd_alias.keys()) == ["Time", "SignalB"]

        cloned = app.clone_window(vis_tbl, new_name="VisUnicodeTblClone")
        assert cloned.get("name") == "VisUnicodeTblClone"

        all_actions = app.list_actions()
        ja_plot_actions = app.list_actions(filter="プロット")
        en_obj_actions = app.list_actions(filter="plot")
        assert len(all_actions) >= 50, f"Expected >= 50 total actions, got {len(all_actions)}"
        assert 1 <= len(ja_plot_actions) < len(all_actions), (
            f"Expected filtered JA actions subset, got {len(ja_plot_actions)}/{len(all_actions)}"
        )
        assert 1 <= len(en_obj_actions) < len(all_actions)

        g_vis = app.plot_xy(vis_tbl, x_col="Time", y_cols=["SignalA", "SignalB"], graph_name="VisUnicodeGraph")
        app.fit_linear(graph=g_vis.name)
        app.fit_nonlinear(table=vis_tbl.name, x_col="Time", y_col="SignalB", model="Gaussian", graph=g_vis.name)

        # Hide VisUnicodeTblClone twice to test both hidden deduplication & .sciprj.gz hidden persistence (#137 P1-3)
        app.activate_window("VisUnicodeTblClone")
        app.trigger_action("hideActiveWindow")
        app.activate_window("VisUnicodeTblClone")
        app.trigger_action("hideActiveWindow")

        # #137 P1-3: Zero duplicate window names & hidden window persistence
        wins_before = app.list_windows()
        names_before = [w["name"] for w in wins_before]
        assert len(names_before) == len(set(names_before)), (
            f"Duplicate window names in list_windows(): {names_before}"
        )
        hidden_before = {w["name"] for w in wins_before if w.get("hidden") is True}
        assert "VisUnicodeTblClone" in hidden_before, f"Expected VisUnicodeTblClone hidden, got {wins_before}"

        # #137 P0-3 & P1-1: Non-ACP Unicode (emoji + Hangul + Japanese) RFC 1952 .sciprj.gz save & reload
        unicode_dir = tmp_dir / "unicode_🧪_한글_日本語_dir"
        unicode_dir.mkdir(parents=True, exist_ok=True)
        gz_path = unicode_dir / "project_압축_テスト.sciprj.gz"

        app.save_project(gz_path, compress=True)
        assert gz_path.is_file() and gz_path.stat().st_size > 200
        raw_gz = gz_path.read_bytes()
        assert raw_gz[:2] == b"\x1f\x8b", (
            f"Compressed .sciprj.gz missing RFC 1952 gzip magic 0x1f 0x8b! Got {raw_gz[:8]!r}"
        )
        decompressed_text = gzip.decompress(raw_gz).decode("utf-8", errors="strict")
        assert "SciDAVis" in decompressed_text and "VisUnicodeTbl" in decompressed_text

        app.new_project()
        assert len(app.list_windows()) == 0

        t_open = time.monotonic()
        app.open_project(gz_path)
        assert (time.monotonic() - t_open) < 8.0, "Unicode .sciprj.gz open_project took too long!"

        wins_after = app.list_windows()
        names_after = [w["name"] for w in wins_after]
        assert len(names_after) == len(set(names_after)), (
            f"Duplicate window names after .sciprj.gz reload: {names_after}"
        )
        assert "VisUnicodeTbl" in names_after and "VisUnicodeGraph" in names_after
        hidden_after = {w["name"] for w in wins_after if w.get("hidden") is True}
        assert hidden_before.issubset(hidden_after), (
            f"Hidden windows lost hidden=True across .sciprj.gz round-trip! before={hidden_before}, after={hidden_after}"
        )

        # Corrupt .sciprj WITH an adjacent .sciprj~ backup file must fail fast without QMessageBox hang!
        corrupt_path = unicode_dir / "corrupt_壊れた.sciprj"
        backup_path = unicode_dir / "corrupt_壊れた.sciprj~"
        corrupt_path.write_bytes(b"NOT_A_SCIPRJ_FILE\x00\xff\xfeCORRUPT_HEADER\n")
        backup_path.write_text(decompressed_text, encoding="utf-8")

        t_corrupt = time.monotonic()
        corrupt_rejected = False
        try:
            app.open_project(corrupt_path)
        except tg.RpcError:
            corrupt_rejected = True
        assert corrupt_rejected, "Corrupt .sciprj did not raise RpcError!"
        assert (time.monotonic() - t_corrupt) < 4.0, (
            "Corrupt .sciprj with .sciprj~ backup hung waiting on modal prompt!"
        )

        # Duplicate window names on append=True must complete cleanly without modal prompt!
        app.open_project(gz_path)
        t_append = time.monotonic()
        app.open_project(gz_path, append=True)
        assert (time.monotonic() - t_append) < 8.0, "open_project(..., append=True) hung!"

    runner.check(
        "8.2 Unicode Emoji/Hangul RFC 1952 .sciprj.gz, Corrupt+Backup Guard, Append & Hidden Persistence",
        _unicode_gzip_sciprj_corrupt_append_and_hidden,
    )

    def _bars_robust_font_3d_emf_graphname_and_403() -> None:
        app.new_project()

        # #137 P0-5: 1-row vertical_bars PNG export
        t1_bar = app.create_table(rows=1, col_names=["X", "Y"], designations=["X", "Y"], name="Bar1RowTbl")
        t1_bar.set_data_dict({"X": [1.0], "Y": [25.0]})
        g_bar1 = app.plot_xy(t1_bar, x_col="X", y_cols=["Y"], style="vertical_bars", graph_name="Bar1RowGraph")
        assert g_bar1.name == "Bar1RowGraph"
        bar1_png = tmp_dir / "one_row_vertical_bars.png"
        g_bar1.export(bar1_png, dpi=100)
        assert bar1_png.is_file() and 200 < bar1_png.stat().st_size < 3_000_000
        meta_b1 = parse_png_metadata_and_pixels(bar1_png)
        assert 50 < meta_b1["width"] < 1800 and 50 < meta_b1["height"] < 1800 and meta_b1["non_white_interior"] > 20

        # #137 P1-7: plot_stacked_bars + bar_gap / bar_offset
        t_sb = app.create_table(
            rows=4, col_names=["Cat", "PartA", "PartB"], designations=["X", "Y", "Y"], name="StackedBarTbl"
        )
        t_sb.set_data_dict({"Cat": [1.0, 2.0, 3.0, 4.0], "PartA": [10.0, 15.0, 12.0, 18.0], "PartB": [5.0, 8.0, 6.0, 9.0]})
        g_sb = app.plot_stacked_bars(t_sb, x_col="Cat", y_cols=["PartA", "PartB"], bar_gap=30, graph_name="StackedBarsGraph")
        assert g_sb.name == "StackedBarsGraph"
        assert len(g_sb.info()["layers"][0]["curves"]) >= 2
        g_sb.set_curve_style(curve_index=0, bar_gap=20, bar_offset=10)

        # #137 P1-8: fit_robust("siegel" & "theil_sen") plotted curves + unknown method -> -32602
        t_rob = app.create_table(rows=10, col_names=["X", "Y"], designations=["X", "Y"], name="RobCurveTbl")
        rx = [float(i) for i in range(10)]
        ry = [2.5 * x + 4.0 for x in rx]
        ry[3] = 500.0  # extreme outlier
        t_rob.set_data_dict({"X": rx, "Y": ry})
        g_rob = app.plot_xy(t_rob, x_col="X", y_cols=["Y"], style="scatter", graph_name="RobCurveGraph")
        c0 = len(g_rob.info()["layers"][0]["curves"])

        res_siegel = app.fit_robust(graph=g_rob.name, method="siegel")
        c1 = len(g_rob.info()["layers"][0]["curves"])
        assert abs(res_siegel["slope"] - 2.5) < 0.15 and c1 == c0 + 1, (
            f"siegel fit failed or did not plot curve: {res_siegel}, curves {c0}->{c1}"
        )

        res_ts = app.fit_robust(graph=g_rob.name, method="theil_sen")
        c2 = len(g_rob.info()["layers"][0]["curves"])
        assert abs(res_ts["slope"] - 2.5) < 0.15 and c2 == c1 + 1, (
            f"theil_sen fit failed or did not plot curve: {res_ts}, curves {c1}->{c2}"
        )

        bad_rob = False
        try:
            app.fit_robust(graph=g_rob.name, method="nonexistent_robust_estimator")
        except tg.RpcError as exc:
            assert exc.code == -32602, f"Expected -32602 for invalid robust method, got {exc}"
            bad_rob = True
        assert bad_rob, "Unknown fit_robust method did not raise RpcError(-32602)"

        # #136 P2-3: Custom font_family preserved on subsequent set_axis when font_family is omitted
        g_rob.set_axis("x", font_family="Times New Roman", font_size=13)
        g_rob.set_axis("x", min=-1.0, max=11.0)
        axes_info = g_rob.info()["layers"][0]["axes"]
        x_axis_info = axes_info.get("x", axes_info.get("bottom", {}))
        font_fam = x_axis_info.get("font_family", x_axis_info.get("title_font_family", ""))
        assert font_fam == "Times New Roman", (
            f"set_axis without font_family clobbered custom font 'Times New Roman' -> {x_axis_info}"
        )

        # #137 P1-6: 3D set_3d_options + 3D .emf export
        mat3d = app.create_matrix(rows=16, cols=16, name="Mat3DEmf", x_min=-2.0, x_max=2.0, y_min=-2.0, y_max=2.0)
        mat3d.set_data(
            [[math.sin(0.4 * r) * math.cos(0.4 * c) for c in range(16)] for r in range(16)]
        )
        g3d = app.plot_3d_matrix(mat3d, style="surface", graph_name="Custom3DSurfEmf")
        assert g3d.name == "Custom3DSurfEmf"
        g3d.set_3d_options(
            title="3D Strain Surface",
            palette="viridis",
            z_scale=1.15,
            z_range=(-1.5, 1.5),
            legend=True,
            mesh_line_width=1.0,
            smooth_mesh=True,
        )
        bad_3d_opt = False
        try:
            app.call("graph.set_3d_options", graph=g3d.name, unknown_3d_key=42)
        except tg.RpcError as exc:
            assert exc.code == -32602, f"Expected -32602 for unknown set_3d_options key, got {exc}"
            bad_3d_opt = True
        assert bad_3d_opt, "Unknown set_3d_options key failed to raise RpcError(-32602)"

        emf_3d_path = tmp_dir / "surface_3d_export.emf"
        g3d.export(emf_3d_path, dpi=200)
        assert emf_3d_path.is_file() and emf_3d_path.stat().st_size > 2000
        emf_bytes = emf_3d_path.read_bytes()
        assert emf_bytes[:4] == b"\x01\x00\x00\x00" and emf_bytes[40:44] == b" EMF", (
            "3D exported .emf missing valid Windows ENHMETAHEADER signature!"
        )

        # #137 P1-9: graph_name honored across all plot.* endpoints
        t_gn = app.create_table(
            rows=8,
            col_names=["A", "B", "C", "D"],
            designations=["X", "Y", "Y", "Y"],
            name="GraphNameAllTbl",
        )
        t_gn.set_data_dict(
            {
                "A": [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0],
                "B": [10.0, 20.0, 15.0, 30.0, 25.0, 35.0, 28.0, 40.0],
                "C": [5.0, 12.0, 18.0, 22.0, 19.0, 27.0, 31.0, 33.0],
                "D": [85.0, 68.0, 77.0, 48.0, 56.0, 38.0, 41.0, 27.0],
            }
        )
        gn_checks = [
            ("GName-DoubleY", lambda: app.plot_double_y(t_gn, x_col="A", y1_col="B", y2_col="C", graph_name="GName-DoubleY")),
            ("GName-Waterfall", lambda: app.plot_waterfall(t_gn, x_col="A", y_cols=["B", "C"], graph_name="GName-Waterfall")),
            ("GName-Hist", lambda: app.plot_histogram(t_gn, cols="B", graph_name="GName-Hist")),
            ("GName-Box", lambda: app.plot_box(t_gn, cols=["B", "C"], graph_name="GName-Box")),
            ("GName-Violin", lambda: app.plot_violin(t_gn, cols=["B", "C"], graph_name="GName-Violin")),
            ("GName-Pareto", lambda: app.plot_pareto(t_gn, col="B", graph_name="GName-Pareto")),
            ("GName-QQ", lambda: app.plot_qq(t_gn, col="B", graph_name="GName-QQ")),
            ("GName-Pie", lambda: app.plot_pie(t_gn, col="B", graph_name="GName-Pie")),
            ("GName-Vectors", lambda: app.plot_vectors(t_gn, cols=["A", "B", "C", "D"], mode="xyxy", graph_name="GName-Vectors")),
            ("GName-Ternary", lambda: app.plot_ternary(t_gn, a_col="B", b_col="C", c_col="D", graph_name="GName-Ternary")),
            ("GName-Func", lambda: app.plot_function("sin(x)", x_min=0.0, x_max=6.28, graph_name="GName-Func")),
            ("GName-Matrix", lambda: app.plot_matrix(mat3d, style="colormap", graph_name="GName-Matrix")),
            ("GName-3DXYZ", lambda: app.plot_3d_xyz(t_gn, x_col="A", y_col="B", z_col="C", style="scatter", graph_name="GName-3DXYZ")),
            ("GName-3DFunc", lambda: app.plot_3d_function("cos(x)*sin(y)", graph_name="GName-3DFunc")),
        ]
        for expected_gn, maker in gn_checks:
            gp = maker()
            assert gp.name == expected_gn, f"plot.* ignored graph_name={expected_gn!r}, returned {gp.name!r}"

        # #137 P1-4: HTTP 403 GitHub rate-limit JSON message surfaced in app.check_updates
        class _Mock403Handler(http.server.BaseHTTPRequestHandler):
            def do_GET(self) -> None:  # noqa: N802
                body = json.dumps(
                    {"message": "API rate limit exceeded for 127.0.0.1 (Python E2E check)"}
                ).encode("utf-8")
                self.send_response(403)
                self.send_header("Content-Type", "application/json; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def log_message(self, format: str, *args: Any) -> None:  # noqa: A002
                pass

        httpd403 = http.server.HTTPServer(("127.0.0.1", 0), _Mock403Handler)
        port403 = httpd403.server_address[1]
        th403 = threading.Thread(target=httpd403.serve_forever, daemon=True)
        th403.start()
        try:
            upd403 = app.check_updates(
                feed_url=f"http://127.0.0.1:{port403}/releases/latest",
                current_version="2.4.0",
            )
            assert upd403.get("ok") is False, f"Expected ok=False on HTTP 403, got {upd403}"
            err_msg = str(upd403.get("error", ""))
            assert "403" in err_msg and "API rate limit exceeded" in err_msg, (
                f"HTTP 403 GitHub JSON message missing in app.check_updates: {upd403}"
            )
        finally:
            httpd403.shutdown()
            httpd403.server_close()

    runner.check(
        "8.3 1-Row Bars, Stacked Bars, Siegel/Theil-Sen Curves, Font Preserve, 3D EMF, graph_name & HTTP 403",
        _bars_robust_font_3d_emf_graphname_and_403,
    )


def run_workflow_8b_named_pipe_cli(runner: E2ERunner, tmp_dir: Path) -> None:
    def _named_pipe_launch_and_timeout() -> None:
        exe_candidates = [
            REPO_ROOT / "build" / "scidavis" / "Release" / "ToriGraph.exe",
            REPO_ROOT / "build" / "scidavis" / "ToriGraph.exe",
        ]
        exe = next((p for p in exe_candidates if p.is_file()), None)
        assert exe is not None, f"ToriGraph.exe not found in {exe_candidates}"

        # 1. Non-existent pipe honors timeout promptly
        t_miss = time.monotonic()
        pipe_failed = False
        try:
            tg.connect(f"pipe://torigraph_nonexistent_{os.getpid()}_missing", timeout=0.35)
        except tg.ToriGraphError:
            pipe_failed = True
        elapsed_miss = time.monotonic() - t_miss
        assert pipe_failed and elapsed_miss < 3.0, (
            f"Missing named pipe connect did not fail within timeout (elapsed={elapsed_miss:.2f}s)"
        )

        # 2. --rpc-pipe without --rpc-port writes pipe://<name> to --rpc-port-file and serves RPC
        pipe_name = f"torigraph_e2e_pipe_{os.getpid()}_{random.randint(1000, 9999)}"
        ep_file = tmp_dir / "named_pipe_endpoint.txt"
        env = os.environ.copy()
        deps_bin = REPO_ROOT / ".deps" / "Library" / "bin"
        env["PATH"] = f"{exe.parent}{os.pathsep}{deps_bin}{os.pathsep}{env.get('PATH', '')}"

        proc = subprocess.Popen(
            [str(exe), "--headless", "--rpc-pipe", pipe_name, "--rpc-port-file", str(ep_file)],
            cwd=str(exe.parent),
            env=env,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        try:
            deadline = time.monotonic() + 15.0
            ep_text = ""
            while time.monotonic() < deadline:
                if ep_file.is_file():
                    ep_text = ep_file.read_text(encoding="utf-8").strip()
                    if ep_text:
                        break
                assert proc.poll() is None, "ToriGraph --rpc-pipe exited prematurely!"
                time.sleep(0.05)

            assert ep_text == f"pipe://{pipe_name}", (
                f"--rpc-pipe wrote {ep_text!r} instead of 'pipe://{pipe_name}'!"
            )

            with tg.connect(ep_text, timeout=5.0) as pipe_app:
                assert pipe_app.ping().get("pong") is True
                pt = pipe_app.create_table(rows=4, col_names=["X", "Y"], designations=["X", "Y"], name="PipeTbl")
                pt.set_data_dict({"X": [1.0, 2.0, 3.0, 4.0], "Y": [10.0, 20.0, 30.0, 40.0]})
                st = pt.stats("Y")
                assert abs(st["mean"] - 25.0) < 1e-9
        finally:
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=5.0)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait(timeout=2.0)

    runner.check(
        "8.4 Win32 Named Pipe (`--rpc-pipe` + `--rpc-port-file` + `_LocalPipeTransport` Timeout)",
        _named_pipe_launch_and_timeout,
    )


def run_workflow_9_round3_critic_regressions(
    runner: E2ERunner, app: tg.Session, tmp_dir: Path
) -> None:
    def _w9_1_set_3d_options_ranges_and_degenerate_matrices() -> None:
        # [P0-R2-1] 1. plot_3d_function + set_3d_options(x_range, y_range, z_range) must not segfault
        g3_fn = app.plot_3d_function("sin(x)*cos(y)", -3.0, 3.0, -3.0, 3.0, graph_name="W9_3DFunc")
        assert g3_fn.name == "W9-3DFunc"
        g3_fn.set_3d_options(
            x_range=(-2.0, 2.0),
            y_range=(-2.0, 2.0),
            z_range=(-1.2, 1.2),
            title="Rescaled 3D Function Surface",
            palette="viridis",
            legend=True,
        )
        assert app.ping().get("pong") is True
        fn_png = tmp_dir / "w9_3d_func.png"
        fn_emf = tmp_dir / "w9_3d_func.emf"
        g3_fn.export(fn_png, dpi=150)
        g3_fn.export(fn_emf, dpi=150)
        assert fn_png.is_file() and fn_png.stat().st_size > 2000
        emf_raw = fn_emf.read_bytes()
        assert emf_raw[:4] == b"\x01\x00\x00\x00" and emf_raw[40:44] == b" EMF"

        # [P0-R2-1] 2. Degenerate 1xN and Nx1 3D matrices must not throw std::bad_array_new_length
        m_1x4 = app.create_matrix(rows=1, cols=4, name="W9_Mat_1x4", x_min=0.0, x_max=3.0, y_min=0.0, y_max=1.0)
        assert m_1x4.name == "W9_Mat_1x4"
        m_1x4.set_data([[1.0, 2.5, 4.0, 3.2]])
        g3_1x4 = m_1x4.plot_3d(graph_name="W9_3D_1x4")
        g3_1x4.set_3d_options(x_range=(0.0, 6.0), y_range=(-1.0, 2.0), z_range=(0.0, 5.0), legend=True)

        m_4x1 = app.create_matrix(rows=4, cols=1, name="W9_Mat_4x1", x_min=0.0, x_max=1.0, y_min=0.0, y_max=3.0)
        m_4x1.set_data([[0.5], [1.5], [3.5], [4.5]])
        g3_4x1 = m_4x1.plot_3d(graph_name="W9_3D_4x1")
        g3_4x1.set_3d_options(z_range=(-1.0, 6.0))

        # [P0-R2-1] 3. Table-backed plot_3d_xyz range & legend sync
        t_xyz = app.create_table(
            rows=6, col_names=["X3", "Y3", "Z3"], designations=["X", "Y", "Z"], name="W9_Tbl3DXYZ"
        )
        t_xyz.set_data_dict(
            {
                "X3": [0.0, 1.0, 2.0, 0.5, 1.5, 2.5],
                "Y3": [0.0, 0.5, 1.0, 1.5, 2.0, 2.5],
                "Z3": [5.0, 12.0, 25.0, 18.0, 30.0, 42.0],
            }
        )
        g3_xyz = app.plot_3d_xyz(t_xyz, x_col="X3", y_col="Y3", z_col="Z3", style="trajectory", graph_name="W9_3DXYZ")
        g3_xyz.set_3d_options(x_range=(-1.0, 4.0), y_range=(-1.0, 4.0), z_range=(0.0, 50.0), legend=True)

        # [P0-R2-1] 4. Inverted range (min >= max) must fail cleanly with RpcError(-32602)
        inv_raised = False
        try:
            g3_fn.set_3d_options(z_range=(5.0, -5.0))
        except tg.RpcError as exc:
            assert exc.code == -32602, f"Expected -32602 on inverted z_range, got {exc}"
            inv_raised = True
        assert inv_raised, "Inverted z_range=(5.0, -5.0) did not raise RpcError(-32602)"

    runner.check(
        "9.1 `[P0-R2-1]` `set_3d_options` Ranges on `3d_function`, `1xN`/`Nx1` `3d_matrix` & `3d_xyz`",
        _w9_1_set_3d_options_ranges_and_degenerate_matrices,
    )

    def _w9_2_headless_window_naming_sanitization_and_collisions() -> None:
        # [P0-R2-2] 1. Underscore preservation + duplicate auto-uniquification on create_table / create_matrix
        t_u1 = app.create_table(rows=5, col_names=["ColXW9", "ColY_W9"], designations=["X", "Y"], name="W9_Under_Tbl")
        assert t_u1.name == "W9_Under_Tbl", f"Expected underscore preserved, got {t_u1.name!r}"
        t_u1.set_data_dict({"ColXW9": [1.0, 2.0, 3.0, 4.0, 5.0], "ColY_W9": [2.0, 4.0, 6.0, 8.0, 10.0]})

        t_u2 = app.create_table(rows=3, col_names=["A", "B"], name="W9_Under_Tbl")
        assert t_u2.name != t_u1.name and t_u2.name.startswith("W9_Under_Tbl"), (
            f"Duplicate create_table did not auto-uniquify: {t_u2.name!r}"
        )

        # [P0-R2-2] 2. Non-\w spaces/parens/Unicode in graph_name must complete fast without modal QMessageBox
        t0 = time.monotonic()
        g_sp = app.plot_xy(t_u1, "ColXW9", "ColY_W9", graph_name="Fig 1 (7075-T6 合金)")
        assert (time.monotonic() - t0) < 3.0 and g_sp.name, f"Non-\\w graph_name failed: {g_sp.name!r}"

        # [P0-R2-2] 3. Repeated graph_name with underscores ('_' -> '-' normalization collision)
        t0 = time.monotonic()
        g_dup1 = app.plot_xy(t_u1, "ColXW9", "ColY_W9", graph_name="Dup_Graph_W9")
        g_dup2 = app.plot_xy(t_u1, "ColXW9", "ColY_W9", graph_name="Dup_Graph_W9")
        assert (time.monotonic() - t0) < 3.0
        assert g_dup1.name == "Dup-Graph-W9" and g_dup2.name != g_dup1.name, (
            f"Duplicate underscore graph_name collided: {g_dup1.name!r} vs {g_dup2.name!r}"
        )

        # [P0-R2-2] 4. Repeated clone_window with colliding new_name
        t0 = time.monotonic()
        cl1 = app.clone_window(t_u1, new_name="Dup_Clone_W9")
        cl2 = app.clone_window(t_u1, new_name="Dup_Clone_W9")
        assert (time.monotonic() - t0) < 3.0
        assert cl1["name"] != cl2["name"], f"Duplicate clone_window collided: {cl1} vs {cl2}"

        # [P0-R2-2] 5. app.rename_window rejects invalid chars, existing window collision, and Table column collision with -32602
        for bad_target in ("Bad Spaces & Parens!", g_dup1.name, "ColXW9"):
            t0 = time.monotonic()
            got_err = False
            try:
                app.rename_window(t_u1, bad_target)
            except tg.RpcError as exc:
                assert exc.code == -32602, f"Expected -32602 for rename_window({bad_target!r}), got {exc}"
                got_err = True
            assert got_err and (time.monotonic() - t0) < 3.0, (
                f"rename_window({bad_target!r}) did not fail fast with -32602!"
            )

        # Valid rename updates proxy .name cleanly
        ren_res = app.rename_window(t_u2, "W9ValidRenamedTbl")
        assert ren_res["name"] == "W9ValidRenamedTbl" and t_u2.name == "W9ValidRenamedTbl"

    runner.check(
        "9.2 `[P0-R2-2]` Headless Window Naming Sanitization, `_` Collision Uniquification & `rename_window` `-32602`",
        _w9_2_headless_window_naming_sanitization_and_collisions,
    )

    def _w9_3_csv_export_import_utf8_rfc4180_and_dir_guard() -> None:
        t_csv = app.create_table(
            rows=4,
            col_names=["Temp_K", "電流_μA_실험", "Sample_Note"],
            designations=["X", "Y", "None"],
            name="W9_Csv_Src",
        )
        temps = [300.0, 400.0, 500.0, 600.0]
        currents = [12500.5, 98765.25, 1234567.891234, -45678.125]
        notes = [
            "Al-Zn-Mg, peak 1",
            'He said "OK" 🧪',
            "α-Fe, 焼き入れ / 열처리",
            "plain_tag",
        ]
        t_csv.set_data_dict(
            {
                "Temp_K": temps,
                "電流_μA_실험": currents,
                "Sample_Note": notes,
            }
        )

        # [P0-R2-3] Exporting to an existing directory must fail fast with RpcError (zero modal QMessageBox hang)
        t0 = time.monotonic()
        dir_failed = False
        try:
            t_csv.export_csv(tmp_dir)
        except tg.RpcError as exc:
            assert exc.code in (-32602, -32000), f"Unexpected code exporting to directory: {exc}"
            dir_failed = True
        assert dir_failed and (time.monotonic() - t0) < 3.0, (
            "t_csv.export_csv(existing_dir) did not fail fast with RpcError!"
        )
        assert app.ping().get("pong") is True

        # [P1-R2-1] Export UTF-8 RFC 4180 CSV with >=10,000 numbers, 13-sig-fig float, JP/KR/Greek/Emoji & quotes/commas
        csv_path = tmp_dir / "w9_utf8_rfc4180_실험_🧪.csv"
        exp_res = t_csv.export_csv(csv_path)
        assert exp_res.get("ok") is True and csv_path.is_file()

        raw_bytes = csv_path.read_bytes()
        csv_text = raw_bytes.decode("utf-8")  # strict UTF-8 decode — zero Shift-JIS / ANSI mojibake
        assert "電流_μA_실험" in csv_text and "??" not in csv_text, (
            f"Exported CSV corrupted UTF-8 JP/KR header: {csv_text!r}"
        )
        assert "12,500" not in csv_text and "98,765" not in csv_text, (
            f"Exported CSV injected locale thousands-separator commas: {csv_text!r}"
        )
        assert "12500.5" in csv_text and "1234567.891234" in csv_text, (
            f"Exported CSV truncated numeric precision: {csv_text!r}"
        )
        assert '"Al-Zn-Mg, peak 1"' in csv_text and '"He said ""OK"" 🧪"' in csv_text, (
            f"Exported CSV missing RFC 4180 quote escaping: {csv_text!r}"
        )

        # Round-trip via app.import_csv (plus duplicate name auto-uniquification check)
        t_re1 = app.import_csv(csv_path, name="W9_Csv_Import")
        t_re2 = app.import_csv(csv_path, name="W9_Csv_Import")
        assert t_re1.name != t_re2.name, f"Duplicate import_csv name collided: {t_re1.name!r}"

        re_cols = t_re1.info()["columns"]
        assert [c["name"] for c in re_cols] == ["Temp_K", "電流_μA_실험", "Sample_Note"], (
            f"Imported CSV column headers mismatch: {re_cols}"
        )
        re_temps = t_re1.get_column(0)
        re_currents = t_re1.get_column(1)
        re_notes = [t_re1.get_cell(r, 2) for r in range(4)]
        for got_t, exp_t in zip(re_temps, temps):
            assert abs(float(got_t) - exp_t) < 1e-9
        for got_c, exp_c in zip(re_currents, currents):
            assert abs(float(got_c) - exp_c) < 1e-6, (
                f"Imported numeric value corrupted ({got_c!r} != {exp_c!r})!"
            )
        assert re_notes == notes, f"Imported RFC 4180 text cells mismatch: {re_notes!r} != {notes!r}"

    runner.check(
        "9.3 `[P0-R2-3]` & `[P1-R2-1]` `export_csv` Dir Guard + UTF-8 JP/KR/Emoji + `>=10000` RFC 4180 `import_csv` Round-Trip",
        _w9_3_csv_export_import_utf8_rfc4180_and_dir_guard,
    )

    def _w9_4_paste_format_fonts_temp_leak_and_append_project_collision() -> None:
        # [P2-R2-1] graph.copy_format + graph.paste_format preserves intentional custom fonts
        t_fmt = app.create_table(rows=5, col_names=["XF", "YF"], designations=["X", "Y"], name="W9_FmtTbl")
        t_fmt.set_data_dict({"XF": [0.0, 1.0, 2.0, 3.0, 4.0], "YF": [1.0, 3.0, 2.0, 5.0, 4.0]})
        g_src = app.plot_xy(t_fmt, "XF", "YF", graph_name="W9_FmtSrc")
        g_dst = app.plot_xy(t_fmt, "XF", "YF", graph_name="W9_FmtDst")

        g_src.set_axis("x", font_family="Times New Roman", font_size=15)
        g_src.set_axis("y", font_family="Times New Roman", font_size=15)
        g_src.set_legend(font_family="Times New Roman", font_size=13)
        g_src.copy_format()
        g_dst.paste_format()

        dst_layer = g_dst.info()["layers"][0]
        dst_x_font = dst_layer["axes"]["x"]["font_family"]
        dst_y_font = dst_layer["axes"]["y"]["font_family"]
        dst_leg_font = dst_layer.get("legend_font_family", "")
        assert dst_x_font == "Times New Roman" and dst_y_font == "Times New Roman", (
            f"paste_format clobbered custom axis fonts to {dst_x_font!r} / {dst_y_font!r}!"
        )
        assert dst_leg_font == "Times New Roman", (
            f"paste_format clobbered custom legend font to {dst_leg_font!r}!"
        )

        # [P2-R2-2] Zero %TEMP%\torigraph_save_*.sciprj leaks on save_project(..., compress=True)
        sys_tmp = Path(tempfile.gettempdir())
        before_leaks = {p.name for p in sys_tmp.glob("torigraph_save_*")}
        gz_noleak = tmp_dir / "w9_zero_temp_leak.sciprj.gz"
        app.save_project(gz_noleak, compress=True)
        assert gz_noleak.is_file() and gz_noleak.read_bytes()[:2] == b"\x1f\x8b"
        after_leaks = {p.name for p in sys_tmp.glob("torigraph_save_*")}
        new_leaks = after_leaks - before_leaks
        assert not new_leaks, f"save_project(compress=True) leaked temp files in %TEMP%: {new_leaks}"

        # [P1-R2-2] open_project(..., append=True) colliding Table1 / Matrix1 curve & spectrogram binding
        proj_b_path = tmp_dir / "w9_append_proj_b.sciprj"
        app.new_project()

        # Build Project B with colliding default names 'Table1' and 'Matrix1'
        t_b = app.create_table(
            rows=5,
            col_names=["1", "2", "strainB"],
            designations=["X", "Y", "Y"],
            name="Table1",
        )
        assert t_b.name == "Table1"
        t_b.set_data_dict(
            {
                "1": [0.0, 1.0, 2.0, 3.0, 4.0],
                "2": [900.0, 925.0, 950.0, 975.0, 999.0],
                "strainB": [500.0, 600.0, 700.0, 800.0, 888.0],
            }
        )
        g_b_def = app.plot_xy(t_b, "1", "2", graph_name="GraphB_DefCols")
        g_b_cus = app.plot_xy(t_b, "1", "strainB", graph_name="GraphB_CustomCol")
        assert g_b_def.name == "GraphB-DefCols" and g_b_cus.name == "GraphB-CustomCol"

        m_b = app.create_matrix(rows=4, cols=4, name="Matrix1", x_min=0.0, x_max=3.0, y_min=0.0, y_max=3.0)
        assert m_b.name == "Matrix1"
        m_b.set_data(
            [
                [100.0, 200.0, 300.0, 400.0],
                [200.0, 400.0, 500.0, 600.0],
                [300.0, 500.0, 700.0, 750.0],
                [400.0, 600.0, 750.0, 777.0],
            ]
        )
        g_b_hm = m_b.plot_heatmap(graph_name="GraphB_Heatmap")
        assert g_b_hm.name == "GraphB-Heatmap"
        app.save_project(proj_b_path, compress=False)

        # Build Project A with its OWN 'Table1' (y_max=33.0) and 'Matrix1' (z_max=11.0)
        app.new_project()
        t_a = app.create_table(rows=5, col_names=["1", "2"], designations=["X", "Y"], name="Table1")
        assert t_a.name == "Table1"
        t_a.set_data_dict({"1": [0.0, 1.0, 2.0, 3.0, 4.0], "2": [10.0, 15.0, 20.0, 25.0, 33.0]})
        g_a = app.plot_xy(t_a, "1", "2", graph_name="GraphA_Orig")
        assert g_a.name == "GraphA-Orig"

        m_a = app.create_matrix(rows=4, cols=4, name="Matrix1", x_min=0.0, x_max=3.0, y_min=0.0, y_max=3.0)
        assert m_a.name == "Matrix1"
        m_a.set_data([[float(r + c + 5) for c in range(4)] for r in range(4)])  # max = 11.0

        # Append Project B into Project A!
        app.open_project(proj_b_path, append=True)

        # 1. Project A's original Table1 & GraphA-Orig must remain untouched (y_max == 33.0)
        assert abs(max(app.table("Table1").get_column("2")) - 33.0) < 1e-9
        ga_curves = app.graph("GraphA-Orig").info()["layers"][0]["curves"]
        assert len(ga_curves) == 1 and abs(ga_curves[0]["y_max"] - 33.0) < 1e-9

        # 2. Appended GraphB-CustomCol must keep its curve (curve_count == 1) with y_max == 888.0
        gb_cus_curves = app.graph("GraphB-CustomCol").info()["layers"][0]["curves"]
        assert len(gb_cus_curves) == 1, (
            f"open_project(append=True) dropped custom-column curve on colliding Table1: {gb_cus_curves}"
        )
        assert abs(gb_cus_curves[0]["y_max"] - 888.0) < 1e-9, (
            f"GraphB-CustomCol y_max mismatch: {gb_cus_curves[0]}"
        )

        # 3. Appended GraphB-DefCols must bind to Project B's appended table (y_max == 999.0), NOT Project A's Table1 (33.0)!
        gb_def_curves = app.graph("GraphB-DefCols").info()["layers"][0]["curves"]
        assert len(gb_def_curves) == 1, f"GraphB-DefCols missing curve: {gb_def_curves}"
        assert abs(gb_def_curves[0]["y_max"] - 999.0) < 1e-9 and gb_def_curves[0]["y_column"] != "Table1_2", (
            f"GraphB-DefCols hijacked Project A's Table1 instead of appended table: {gb_def_curves[0]}"
        )

        # 4. Appended GraphB-Heatmap must bind to Project B's appended matrix (z_max == 777.0), NOT Project A's Matrix1 (11.0)!
        gb_specs = app.graph("GraphB-Heatmap").info()["layers"][0]["spectrograms"]
        assert len(gb_specs) == 1, f"GraphB-Heatmap missing spectrogram: {gb_specs}"
        assert abs(gb_specs[0]["z_max"] - 777.0) < 1e-6 and gb_specs[0]["matrix"] != "Matrix1", (
            f"GraphB-Heatmap hijacked Project A's Matrix1 instead of appended matrix: {gb_specs[0]}"
        )

    runner.check(
        "9.4 `[P1-R2-2]` `open_project(append=True)` Collision Binding + `[P2-R2-1]` `paste_format` Fonts + `[P2-R2-2]` `0` `%TEMP%` Leaks",
        _w9_4_paste_format_fonts_temp_leak_and_append_project_collision,
    )


def run_workflow_10_round4_unified_regressions(
    runner: E2ERunner, app: tg.Session, tmp_dir: Path
) -> None:
    """Section 25: Round 4 Unified Critic & QA-Shard Regression Gate (#166-#170 / #176-#180)."""

    def _w10_round4_unified_regression_gate() -> None:
        # ------------------------------------------------------------------
        # A. 3D Underscore-Named Table (CT_Strain_Raw), set_3d_options,
        #    Cell Edit, First-Call Headless UTF-8/Arial .svg/.pdf on Non-ACP
        #    Unicode Path (_wfopen), .sciprj Reload & Invalid 3D Formula Guard
        # ------------------------------------------------------------------
        app.new_project()
        t3d = app.create_table(
            rows=8,
            cols=3,
            name="CT_Strain_Raw",
            col_names=["X_pos", "Y_pos", "Z_val"],
            designations=["X", "Y", "Z"],
        )
        t3d.set_column("X_pos", [float(i) * 1.25 for i in range(8)])
        t3d.set_column("Y_pos", [float(i) * 1.75 + 0.5 for i in range(8)])
        t3d.set_column("Z_val", [float(i * i) + 4.0 for i in range(8)])

        g3d = app.plot_3d_xyz(
            t3d,
            x_col="X_pos",
            y_col="Y_pos",
            z_col="Z_val",
            style="scatter",
            graph_name="G3D-Underscore",
        )
        g3d.set_3d_options(
            axes_labels=["X軸", "Y軸", "Z軸"],
            x_range=[-1.0, 12.0],
            y_range=[-1.0, 18.0],
            z_range=[0.0, 80.0],
        )
        t3d.set_cell(7, "Z_val", 68.5)
        g3d_info = g3d.info()
        assert g3d_info.get("type") == "Graph3D", f"Unexpected 3D graph info: {g3d_info}"

        uni_svg = tmp_dir / "r4_3d_č_€_テスト.svg"
        uni_pdf = tmp_dir / "r4_3d_č_€_テスト.pdf"
        g3d.export(uni_svg)
        g3d.export(uni_pdf)
        assert uni_svg.is_file() and uni_svg.stat().st_size > 500, "3D Unicode SVG missing/empty"
        svg_raw = uni_svg.read_bytes()
        assert b"\xff" not in svg_raw, "3D SVG contains Latin-1 0xFF byte corruption"
        svg_txt = svg_raw.decode("utf-8")
        assert "<svg" in svg_txt and "<text" in svg_txt, (
            "First-call headless 3D SVG missing <svg> or <text> elements!"
        )
        assert "Arial" in svg_txt and "Courier" not in svg_txt, (
            "3D SVG violated Arial font gate (found Courier or missing Arial)!"
        )
        assert "X軸" in svg_txt, "3D SVG missing UTF-8 Japanese axis label 'X軸'!"

        assert uni_pdf.is_file() and uni_pdf.stat().st_size > 800, "3D Unicode PDF missing/empty"
        assert uni_pdf.read_bytes().startswith(b"%PDF-"), "3D Unicode PDF missing %PDF- header"

        sciprj_3d = tmp_dir / "r4_3d_underscore.sciprj"
        app.save_project(sciprj_3d)
        app.new_project()
        app.open_project(sciprj_3d)
        win_names = {w["name"] for w in app.list_windows()}
        assert "CT_Strain_Raw" in win_names and "G3D-Underscore" in win_names, (
            f"Reloaded 3D project missing CT_Strain_Raw or G3D-Underscore: {win_names}"
        )

        got_bad_3d = False
        try:
            app.plot_3d_function("sin(x) + * @@@", x_min=-2.0, x_max=2.0, y_min=-2.0, y_max=2.0)
        except tg.RpcError as exc:
            got_bad_3d = exc.code == -32602
        assert got_bad_3d, "Invalid plot_3d_function syntax must raise RpcError(-32602)!"

        # ------------------------------------------------------------------
        # B. GUI-Mode (headless=False) ScopedNonInteractiveGuard & Modal
        #    trigger_action Protection
        # ------------------------------------------------------------------
        with tg.launch(headless=False, language="ja") as gui_app:
            gt = gui_app.create_table(rows=4, cols=2, name="GuiGuardTbl")
            t0 = time.monotonic()
            bad_ren = False
            try:
                gui_app.rename_window(gt, "Invalid Spaces!")
            except tg.RpcError as exc:
                bad_ren = exc.code == -32602
            assert bad_ren and (time.monotonic() - t0) < 5.0, (
                "GUI-mode rename_window failed non-interactive guard!"
            )

            for modal_act in ("actionOpen", "showExportASCIIDialog"):
                t_act = time.monotonic()
                blocked_ok = False
                try:
                    gui_app.trigger_action(modal_act)
                except tg.RpcError as exc:
                    blocked_ok = exc.code in (-32000, -32602)
                assert blocked_ok and (time.monotonic() - t_act) < 5.0, (
                    f"Modal action {modal_act!r} was not blocked immediately in GUI RPC mode!"
                )

            res_chk_f1 = gui_app.trigger_action("actionToggleFocusHudMode", checked=False)
            assert res_chk_f1.get("ok") is True and res_chk_f1.get("checked") is False, (
                f"Idempotent trigger_action(checked=False) inverted state: {res_chk_f1}"
            )
            res_chk_t1 = gui_app.trigger_action("actionToggleFocusHudMode", checked=True)
            assert res_chk_t1.get("ok") is True and res_chk_t1.get("checked") is True, (
                f"trigger_action(checked=True) failed: {res_chk_t1}"
            )
            res_chk_t2 = gui_app.trigger_action("actionToggleFocusHudMode", checked=True)
            assert res_chk_t2.get("ok") is True and res_chk_t2.get("checked") is True, (
                f"Idempotent trigger_action(checked=True) inverted state: {res_chk_t2}"
            )
            res_chk_f2 = gui_app.trigger_action("actionToggleFocusHudMode", checked=False)
            assert res_chk_f2.get("ok") is True and res_chk_f2.get("checked") is False, (
                f"trigger_action(checked=False) failed to exit Focus HUD: {res_chk_f2}"
            )
            res_manual_slot = gui_app.trigger_action("setRecalcModeManual")
            assert res_manual_slot.get("ok") is True, f"Safe setRecalcModeManual slot failed: {res_manual_slot}"
            res_add_fld = gui_app.trigger_action("addFolder")
            assert res_add_fld.get("ok") is True, f"Safe addFolder action failed: {res_add_fld}"

        # ------------------------------------------------------------------
        # C. Chained Table1+Table2 Single-Hop append=True, Multi-Folder
        #    new_project(), Multi-Dot .sciprj.gz Folder Name & Corrupt Rollback
        # ------------------------------------------------------------------
        app.new_project()
        src_t1 = app.create_table(rows=4, cols=2, name="Table1")
        src_t2 = app.create_table(rows=4, cols=2, name="Table2")
        src_t1.set_column(0, [0.0, 1.0, 2.0, 3.0])
        src_t1.set_column(1, [10.0, 20.0, 30.0, 111.0])
        src_t2.set_column(0, [0.0, 1.0, 2.0, 3.0])
        src_t2.set_column(1, [20.0, 40.0, 60.0, 222.0])
        app.plot_xy(src_t1, x_col=0, y_cols=1, graph_name="ChainAppG1")
        app.plot_xy(src_t2, x_col=0, y_cols=1, graph_name="ChainAppG2")

        multi_dot_gz = tmp_dir / "my.study.part1.sciprj.gz"
        app.save_project(multi_dot_gz, compress=True)

        app.trigger_action("addFolder")
        app.trigger_action("addFolder")
        np_res = app.new_project()
        assert np_res.get("window_count") == 0, f"new_project left windows: {np_res}"

        host_t1 = app.create_table(rows=4, cols=2, name="Table1")
        host_t1.set_column(0, [0.0, 1.0, 2.0, 3.0])
        host_t1.set_column(1, [1.0, 2.0, 3.0, 9.0])

        app_res = app.open_project(multi_dot_gz, append=True)
        assert app_res.get("appended_folder") == "my.study.part1", (
            f"Multi-dot .sciprj.gz folder name mismatch: {app_res}"
        )
        c_g1 = app.graph("ChainAppG1").info()["layers"][0]["curves"][0]
        c_g2 = app.graph("ChainAppG2").info()["layers"][0]["curves"][0]
        assert abs(c_g1["y_max"] - 111.0) < 1e-9 and abs(c_g2["y_max"] - 222.0) < 1e-9, (
            f"Chained Table1+Table2 append=True double-hopped curves: g1={c_g1}, g2={c_g2}"
        )

        win_before_corrupt = len(app.list_windows())
        corrupt_sciprj = tmp_dir / "bad_corrupt.sciprj"
        corrupt_sciprj.write_bytes(b"NOT_A_SCIPRJ_FILE\n<<<GARBAGE>>>\n")
        corrupt_opj = tmp_dir / "bad_corrupt.opj"
        corrupt_opj.write_bytes(b"NOT_CPYA_ORIGIN_HEADER\x00\x01\x02")
        for bad_p in (corrupt_sciprj, corrupt_opj):
            rejected = False
            try:
                app.open_project(bad_p, append=True)
            except tg.RpcError as exc:
                rejected = exc.code in (-32000, -32602)
            assert rejected, f"Corrupt {bad_p.name} append=True was not rejected!"
        assert len(app.list_windows()) == win_before_corrupt, (
            "Corrupt append=True leaked orphan windows!"
        )

        # #183 [P0-R4-1]: 6x stress loop of subfolder append + subfolder close_window + new_project + create_table
        for uaf_cyc in range(6):
            app.new_project()
            ht = app.create_table(rows=4, cols=2, name="Table1")
            ht.set_column(0, [0.0, 1.0, 2.0, 3.0])
            ht.set_column(1, [float(uaf_cyc + k) for k in range(4)])
            hm = app.create_matrix(rows=3, cols=3, name="Matrix1")
            hm.set_data([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0], [7.0, 8.0, 9.0]])
            ap_loop = app.open_project(multi_dot_gz, append=True)
            assert ap_loop.get("appended_folder") == "my.study.part1"
            app.close_window("ChainAppG2")
            app.close_window("Table2")
            np_loop = app.new_project()
            assert np_loop.get("window_count") == 0, f"Cycle {uaf_cyc} new_project leaked windows: {np_loop}"

        # ------------------------------------------------------------------
        # D. "raw_V" vs "V" Formula Resolution, tablecol("T", "C") Space
        #    Trimming & Reverse-Order Cross-Table Recalc Modes (manual / auto)
        # ------------------------------------------------------------------
        app.new_project()
        app.set_recalc_mode("auto")
        tf = app.create_table(rows=4, cols=3, name="RawVTbl", col_names=["V", "raw_V", "sum_col"])
        tf.set_column("V", [1.0, 2.0, 3.0, 4.0])
        tf.set_column("raw_V", [100.0, 200.0, 300.0, 400.0])
        tf.set_formula("sum_col", 'col("raw_V") + tablecol("RawVTbl", "V")', recalculate=True)
        got_sum = tf.get_column("sum_col")
        assert all(abs(got_sum[i] - ((i + 1) * 101.0)) < 1e-9 for i in range(4)), (
            f"col('raw_V') / tablecol('RawVTbl', 'V') space-trimming failed: {got_sum}"
        )

        # Create reverse-order chain: Chain_1_Final -> Chain_2_Mid -> Chain_3_Base
        c1_fin = app.create_table(rows=3, cols=1, name="Chain_1_Final", col_names=["F"])
        c2_mid = app.create_table(rows=3, cols=1, name="Chain_2_Mid", col_names=["M"])
        c3_base = app.create_table(rows=3, cols=1, name="Chain_3_Base", col_names=["B"])
        c3_base.set_column("B", [2.0, 3.0, 4.0])
        c2_mid.set_formula("M", 'tablecol("Chain_3_Base", "B") * 10', recalculate=True)
        c1_fin.set_formula("F", 'tablecol("Chain_2_Mid", "M") + 5', recalculate=True)
        assert abs(c1_fin.get_cell(0, "F") - 25.0) < 1e-9

        app.set_recalc_mode("manual")
        c3_base.set_cell(0, "B", 7.0)
        assert abs(c1_fin.get_cell(0, "F") - 25.0) < 1e-9, (
            "Manual recalc mode prematurely updated dependent cross-table formula!"
        )
        app.recalculate()
        assert abs(c2_mid.get_cell(0, "M") - 70.0) < 1e-9 and abs(c1_fin.get_cell(0, "F") - 75.0) < 1e-9, (
            f"Multi-pass app.recalculate() failed on reverse-order chain: M={c2_mid.get_cell(0, 'M')}, F={c1_fin.get_cell(0, 'F')}"
        )

        app.set_recalc_mode("auto")
        c3_base.set_cell(0, "B", 9.0)
        assert abs(c1_fin.get_cell(0, "F") - 95.0) < 1e-9, (
            f"Auto recalc mode failed immediate cross-table propagation: F={c1_fin.get_cell(0, 'F')}"
        )

        # ------------------------------------------------------------------
        # E. MMF / SineSq / DoniachSunjic Auto-Fit, Syntax Guards & Masked Siegel
        # ------------------------------------------------------------------
        app.new_project()
        xs_mmf = [0.4 + 0.3 * i for i in range(40)]
        ys_mmf = [(2.0 * 16.0 + 52.0 * (x ** 2.2)) / (16.0 + (x ** 2.2)) for x in xs_mmf]
        t_mmf = app.create_table(rows=len(xs_mmf), cols=2, name="MmfTbl", col_names=["X", "Y"])
        t_mmf.set_column("X", xs_mmf)
        t_mmf.set_column("Y", ys_mmf)
        res_mmf = app.fit_nonlinear(table=t_mmf, x_col="X", y_col="Y", model="MMF")
        assert res_mmf.get("converged") and res_mmf.get("r2", 0.0) > 0.999, f"MMF auto-fit failed: {res_mmf}"

        xs_ss = [0.125 * i for i in range(80)]
        ys_ss = [1.5 + 8.0 * (math.sin(math.pi * (x - 1.2) / 4.0) ** 2) for x in xs_ss]
        t_ss = app.create_table(rows=len(xs_ss), cols=2, name="SineSqTbl", col_names=["X", "Y"])
        t_ss.set_column("X", xs_ss)
        t_ss.set_column("Y", ys_ss)
        res_ss = app.fit_nonlinear(table=t_ss, x_col="X", y_col="Y", model="SineSq")
        assert res_ss.get("converged") and res_ss.get("r2", 0.0) > 0.999, f"SineSq auto-fit failed: {res_ss}"

        # Doniach-Sunjic XPS peak centered at 284.8 eV (alpha=0.15, gamma=0.7)
        xs_ds = [278.0 + 0.1 * i for i in range(141)]
        alpha_ds, gam_ds, xc_ds = 0.15, 0.7, 284.8
        raw_ds = []
        for x in xs_ds:
            dx = x - xc_ds
            r2_u = dx * dx + gam_ds * gam_ds
            theta = math.atan2(dx, gam_ds)
            val = math.cos(0.5 * math.pi * alpha_ds + (1.0 - alpha_ds) * theta) / (
                r2_u ** (0.5 * (1.0 - alpha_ds))
            )
            raw_ds.append(val)
        pk_max = max(raw_ds)
        ys_ds = [50.0 * (v / pk_max) for v in raw_ds]
        t_ds = app.create_table(rows=len(xs_ds), cols=2, name="DsPeakTbl", col_names=["BE", "Counts"])
        t_ds.set_column("BE", xs_ds)
        t_ds.set_column("Counts", ys_ds)
        res_ds = app.analyze_peaks(
            table=t_ds, x_col="BE", y_col="Counts", profile="DoniachSunjic", num_peaks=1
        )
        assert res_ds.get("ok") and res_ds.get("r2", 0.0) > 0.99, f"DoniachSunjic fit failed: {res_ds}"
        ds_center = res_ds["peaks"][0]["center"]
        assert abs(ds_center - xc_ds) < 0.35, f"DoniachSunjic center mismatch: {ds_center} vs {xc_ds}"

        # Invalid syntax / missing param_names / unknown profile guards
        for fn_bad in (
            lambda: app.fit_nonlinear(table=t_mmf, x_col="X", y_col="Y", formula="a * (x + "),
            lambda: app.fit_global(table=t_mmf, x_col="X", y_cols=["Y"], formula="a*x+b"),
            lambda: app.fit_implicit_odr(table=t_mmf, x_col="X", y_col="Y", formula="x^2 + y^2 - R^2"),
            lambda: app.analyze_peaks(table=t_ds, x_col="BE", y_col="Counts", profile="BogusPeakShape"),
        ):
            raised_32602 = False
            try:
                fn_bad()
            except tg.RpcError as exc:
                raised_32602 = exc.code == -32602
            assert raised_32602, "Invalid syntax/param guard failed to raise RpcError(-32602)!"

        m_surf = app.create_matrix(rows=8, cols=8, name="BadSurfMat")
        m_surf.set_data([[float(r + c) for c in range(8)] for r in range(8)])
        try:
            bad_surf = app.fit_surface_3d(matrix=m_surf, model="custom", formula="a * (x + ")
            assert not bad_surf.get("converged", False) and (bad_surf.get("r2") or 0.0) <= 1.0, (
                f"Invalid custom 3D surface formula claimed convergence or r2>1: {bad_surf}"
            )
        except tg.RpcError as exc:
            assert exc.code in (-32602, -32000)

        # Siegel repeated-median robust fit ignoring masked 45% +1e8 outliers
        t_sieg = app.create_table(rows=20, cols=2, name="SiegelMaskTbl", col_names=["X", "Y"])
        xs_sg = [float(i) for i in range(20)]
        ys_sg = [3.0 * i + 2.0 if i < 11 else 1.0e8 for i in range(20)]
        t_sieg.set_column("X", xs_sg)
        t_sieg.set_column("Y", ys_sg)
        t_sieg.mask_rows(list(range(11, 20)), col="Y")
        res_sieg = app.fit_robust(method="siegel", table=t_sieg, x_col="X", y_col="Y")
        assert abs(res_sieg["slope"] - 3.0) < 0.05, f"Masked Siegel fit failed: {res_sieg}"

        # ------------------------------------------------------------------
        # F. ANOVA Tukey/Games-Howell/Dunnett, Masked Friedman/ROC/Correlation/
        #    Ternary, FFT Curve dt + Odd-N shift=True, Highpass IIR & Image Profile
        # ------------------------------------------------------------------
        app.new_project()
        t_aov = app.create_table(
            rows=8, cols=3, name="AnovaHetTbl", col_names=["Ctrl", "LowDose", "HighDose"]
        )
        t_aov.set_column("Ctrl", [10.0, 10.1, 9.9, 10.05, 9.95, 10.02, 9.98, 10.0])
        t_aov.set_column("LowDose", [12.0, 12.6, 11.5, 12.3, 11.8, 12.4, 11.7, 12.1])
        t_aov.set_column("HighDose", [18.0, 24.0, 13.0, 21.5, 14.5, 23.0, 15.0, 20.0])

        aov_tuk = app.anova_oneway(table=t_aov, cols=["Ctrl", "LowDose", "HighDose"], posthoc="tukey")
        aov_gh = app.anova_oneway(table=t_aov, cols=["Ctrl", "LowDose", "HighDose"], posthoc="games_howell")
        aov_dun = app.anova_oneway(
            table=t_aov, cols=["Ctrl", "LowDose", "HighDose"], posthoc="dunnett", control="Ctrl"
        )
        assert aov_tuk["posthoc"] == "tukey" and len(aov_tuk["comparisons"]) == 3
        assert aov_gh["posthoc"] == "games_howell" and len(aov_gh["comparisons"]) == 3
        assert aov_dun["posthoc"] == "dunnett" and aov_dun["control"] == "Ctrl" and len(aov_dun["comparisons"]) == 2
        assert abs(aov_tuk["comparisons"][0]["se"] - aov_gh["comparisons"][0]["se"]) > 1e-4, (
            "Tukey vs Games-Howell did not compute distinct heteroscedastic SEs!"
        )

        # Friedman with masked outlier block
        t_fr = app.create_table(rows=6, cols=3, name="FriedmanMaskTbl", col_names=["C1", "C2", "C3"])
        t_fr.set_column("C1", [1.0, 1.2, 0.9, 1.1, 1.05, 99999.0])
        t_fr.set_column("C2", [5.0, 5.1, 4.8, 5.2, 4.95, -99999.0])
        t_fr.set_column("C3", [9.0, 9.4, 8.9, 9.2, 9.10, 0.0])
        t_fr.mask_rows([5], col="C1")
        fr_res = app.nonparametric(table=t_fr, cols=["C1", "C2", "C3"], method="friedman")
        assert fr_res["ok"] and fr_res["n_blocks"] == 5 and fr_res["p_value"] < 0.05, (
            f"Masked Friedman test failed: {fr_res}"
        )

        # ROC with string binary labels + inverted score (true AUC < 0.25) + masked row
        t_roc = app.create_table(rows=9, cols=2, name="RocInvTbl", col_names=["Label", "Score"])
        t_roc.set_column("Label", ["neg", "neg", "neg", "neg", "pos", "pos", "pos", "pos", "pos"], mode="text")
        t_roc.set_column("Score", [9.0, 8.5, 8.0, 7.5, 2.0, 1.8, 1.5, 1.2, 999.0])
        t_roc.mask_rows([8], col="Score")
        roc_res = app.roc(table=t_roc, state_col="Label", score_col="Score")
        assert roc_res["ok"] and roc_res["n_pos"] == 4 and roc_res["n_neg"] == 4, f"ROC counts wrong: {roc_res}"
        assert 0.0 <= roc_res["auc"] < 0.25, f"ROC clamped/flipped AUC < 0.5: {roc_res}"

        # Pairwise correlation with NaN/None and masked rows
        t_cor = app.create_table(rows=7, cols=3, name="CorrMaskTbl", col_names=["A", "B", "C"])
        t_cor.set_column("A", [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0])
        t_cor.set_column("B", [2.0, 4.0, 6.0, 8.0, 10.0, 12.0, -5000.0])
        t_cor.set_column("C", [10.0, None, 8.0, 7.0, None, 4.0, 3.0])
        t_cor.mask_rows([6], col="B")
        cor_res = app.correlation(table=t_cor, cols=["A", "B", "C"])
        r_mat = cor_res["r_matrix"]
        assert abs(r_mat[0][1] - 1.0) < 1e-6 and r_mat[0][2] < -0.95, f"Masked/NaN correlation failed: {cor_res}"

        # Ternary plot skipping NaN & masked rows
        t_tern = app.create_table(rows=6, cols=3, name="TernMaskTbl", col_names=["A", "B", "C"])
        t_tern.set_column("A", [20.0, 50.0, 30.0, 10.0, None, 25.0])
        t_tern.set_column("B", [30.0, 25.0, 40.0, 60.0, 50.0, 25.0])
        t_tern.set_column("C", [50.0, 25.0, 30.0, 30.0, 50.0, 50.0])
        t_tern.mask_rows([5], col="A")
        g_tern = app.plot_ternary(t_tern, "A", "B", "C")
        tern_pts = g_tern.info()["layers"][0]["curves"][0]["points"]
        assert tern_pts == 4, f"Ternary plot included NaN/masked rows (expected 4 pts, got {tern_pts})"

        # Signal FFT on odd N=101 curve with dt=0.01 (fs=100 Hz, f0=15 Hz) and shift=True
        n_odd = 101
        dt_s = 0.01
        xs_fft = [i * dt_s for i in range(n_odd)]
        ys_fft = [2.0 * math.cos(2.0 * math.pi * 15.0 * t) for t in xs_fft]
        t_fft_in = app.create_table(rows=n_odd, cols=2, name="OddFftInTbl", col_names=["t", "sig"])
        t_fft_in.set_column("t", xs_fft)
        t_fft_in.set_column("sig", ys_fft)
        g_fft = app.plot_xy(t_fft_in, x_col="t", y_cols="sig", graph_name="OddFftGraph")
        fft_out = app.fft(graph=g_fft, shift=True)
        assert fft_out.get("ok") and fft_out.get("table"), f"Curve FFT failed: {fft_out}"
        fft_tbl = app.table(fft_out["table"])
        fft_cols = fft_tbl.info()["columns"]
        freqs = fft_tbl.get_column(0)
        amps = fft_tbl.get_column(min(3, len(fft_cols) - 1))
        assert min(freqs) < -40.0 and max(freqs) > 40.0, (
            f"Curve FFT failed to infer dt=0.01 (freq range {min(freqs)}..{max(freqs)})!"
        )
        pos_pk = max((amps[i], freqs[i]) for i in range(len(freqs)) if freqs[i] > 1.0)
        neg_pk = max((amps[i], freqs[i]) for i in range(len(freqs)) if freqs[i] < -1.0)
        assert abs(pos_pk[1] - 15.0) <= 1.2 and abs(neg_pk[1] + 15.0) <= 1.2, (
            f"Odd-N shifted FFT peak mismatch: pos={pos_pk}, neg={neg_pk}"
        )
        assert abs(pos_pk[0] - neg_pk[0]) / max(pos_pk[0], 1e-9) < 0.05, (
            f"Odd-N shifted FFT broke conjugate amplitude symmetry: pos={pos_pk}, neg={neg_pk}"
        )

        # Butterworth highpass IIR filter (attenuates 1.5 Hz, preserves 35 Hz at fs=200 Hz)
        n_iir = 400
        xs_iir = [i * 0.005 for i in range(n_iir)]
        ys_iir = [
            5.0 * math.sin(2.0 * math.pi * 1.5 * t) + 2.0 * math.sin(2.0 * math.pi * 35.0 * t)
            for t in xs_iir
        ]
        t_iir = app.create_table(rows=n_iir, cols=2, name="HpIirTbl", col_names=["t", "mix"])
        t_iir.set_column("t", xs_iir)
        t_iir.set_column("mix", ys_iir)
        g_iir = app.plot_xy(t_iir, x_col="t", y_cols="mix", graph_name="HpIirGraph")
        iir_res = app.iir_filter(graph=g_iir, filter_type="highpass", cutoff=15.0, order=4)
        assert iir_res.get("ok") and iir_res.get("pass_type") == "highpass", f"Highpass IIR failed: {iir_res}"
        hp_curves = g_iir.info()["layers"][0]["curves"]
        assert len(hp_curves) >= 2 and 1.5 < hp_curves[-1]["y_max"] < 3.2, (
            f"Highpass IIR did not attenuate 1.5 Hz component (expected ~2.0 amplitude): {hp_curves[-1]}"
        )

        # Gadget validation + image_profile query_gadget(create_table=True)
        bad_gad = False
        try:
            g_iir.attach_gadget("nonexistent_gadget_xyz")
        except tg.RpcError as exc:
            bad_gad = exc.code == -32602
        assert bad_gad, "Unknown gadget type failed to raise RpcError(-32602)!"

        m_prof = app.create_matrix(rows=24, cols=24, name="ProfGadgetMat", x_min=-3.0, x_max=3.0, y_min=-3.0, y_max=3.0)
        m_prof.set_data(
            [
                [
                    10.0 * math.exp(-(((c - 11.5) / 4.0) ** 2 + ((r - 11.5) / 4.0) ** 2))
                    for c in range(24)
                ]
                for r in range(24)
            ]
        )
        g_prof = m_prof.plot_heatmap(graph_name="ProfGadgetHeatmap")
        g_prof.attach_gadget("image_profile")
        prof_q = g_prof.query_gadget("image_profile", create_table=True)
        assert prof_q.get("ok") and prof_q.get("table"), (
            f"image_profile query_gadget(create_table=True) did not create table: {prof_q}"
        )
        prof_tbl_info = app.table(prof_q["table"]).info()
        assert prof_tbl_info.get("rows", 0) >= 10, f"Extracted profile table empty: {prof_tbl_info}"

    runner.check(
        "10. Round 4 Unified Critic & QA-Shard Regression Gate (3D Underscore/UTF-8/_wfopen, GUI RPC Guard, Single-Hop Append, raw_V Formula, Recalc, MMF/SineSq/DS, Tukey/GH/Dunnett, Masked Stats & Odd-N FFT)",
        _w10_round4_unified_regression_gate,
    )


def run_workflow_11_round5_zero_defect_gate(
    runner: E2ERunner, app: tg.Session, tmp_dir: Path
) -> None:
    """Section 26: Round 5 Zero-Defect Gate (#194-#196 / #200-#202)."""

    def _w11_round5_zero_defect_gate() -> None:
        app.new_project()

        # ------------------------------------------------------------------
        # 1. #194 [P1-R4-1]: Multi-curve short-name resolution ("raw_V" vs "V")
        #    across curvemath, signal, analysis & gadget when "raw_V" is curve 0
        # ------------------------------------------------------------------
        t_hj = app.create_table(rows=5, cols=3, col_names=["x", "V", "raw_V"], name="MultiCurveHJ")
        t_hj.set_column("x", [0.0, 1.0, 2.0, 3.0, 4.0])
        t_hj.set_column("V", [0.0, 2.0, 4.0, 6.0, 8.0])
        t_hj.set_column("raw_V", [0.0, 100.0, 200.0, 300.0, 400.0])
        g_hj = app.plot_xy(table=t_hj, x_col="x", y_cols=["raw_V", "V"], graph_name="MultiCurveHJGraph")

        res_int_v = app.integrate_curve(graph=g_hj, curve="V")
        assert res_int_v.get("ok") and abs(float(res_int_v["area"]) - 16.0) < 1e-6, (
            f"integrate_curve(curve='V') hijacked by 'raw_V': {res_int_v}"
        )
        res_int_raw = app.integrate_curve(graph=g_hj, curve="raw_V")
        assert res_int_raw.get("ok") and abs(float(res_int_raw["area"]) - 800.0) < 1e-6, (
            f"integrate_curve(curve='raw_V') mismatch: {res_int_raw}"
        )

        res_lkp_v = app.lookup_xy(graph=g_hj, curve="V", x=2.0)
        assert res_lkp_v.get("ok") and abs(float(res_lkp_v["y"]) - 4.0) < 1e-6, (
            f"lookup_xy(curve='V', x=2.0) hijacked by 'raw_V': {res_lkp_v}"
        )

        res_sg_v = app.savgol_derivative(graph=g_hj, curve="V", window=5, poly_order=2, deriv_order=1)
        assert res_sg_v.get("ok") and res_sg_v.get("output_curve"), (
            f"savgol_derivative(curve='V') failed: {res_sg_v}"
        )
        sg_lkp = app.lookup_xy(graph=g_hj, curve=res_sg_v["output_curve"], x=2.0)
        assert sg_lkp.get("ok") and abs(float(sg_lkp["y"]) - 2.0) < 1e-4, (
            f"savgol_derivative(curve='V') hijacked by 'raw_V' (want 2.0, got {sg_lkp})"
        )

        res_lin_v = app.call("analysis.fit_linear", graph=g_hj.name, curve="V")
        assert res_lin_v.get("ok") and abs(float(res_lin_v["slope"]) - 2.0) < 1e-6, (
            f"analysis.fit_linear(curve='V') hijacked by 'raw_V': {res_lin_v}"
        )

        # ------------------------------------------------------------------
        # 2. #194 [P2-R4-1]: GraphProxy.set_gadget_roi(x=...) / (x1=..., x2=...)
        #    / (x=..., y=...) / positional (x_min, x_max)
        # ------------------------------------------------------------------
        g_hj.attach_gadget("vertical_cursor", x=1.0)
        vc_set = g_hj.set_gadget_roi(x=3.0)
        assert vc_set.get("ok"), f"g.set_gadget_roi(x=3.0) failed: {vc_set}"
        vc_q = g_hj.query_gadget("vertical_cursor")
        assert abs(float(vc_q.get("x", -1.0)) - 3.0) < 1e-6, f"vertical_cursor x mismatch: {vc_q}"
        g_hj.clear_gadgets()

        g_hj.attach_gadget("delta_cursor", x1=0.0, x2=2.0)
        dc_set = g_hj.set_gadget_roi(x1=1.0, x2=4.0)
        assert dc_set.get("ok"), f"g.set_gadget_roi(x1=1.0, x2=4.0) failed: {dc_set}"
        dc_q = g_hj.query_gadget("delta_cursor")
        assert abs(abs(float(dc_q.get("dx", 0.0))) - 3.0) < 1e-6, f"delta_cursor dx mismatch: {dc_q}"
        g_hj.clear_gadgets()

        g_hj.attach_gadget("roi_stats", x_min=0.0, x_max=2.0)
        roi_set = g_hj.set_gadget_roi(1.0, 4.0)
        assert roi_set.get("ok"), f"g.set_gadget_roi(1.0, 4.0) failed: {roi_set}"
        g_hj.clear_gadgets()

        m_gp = app.create_matrix(rows=16, cols=16, name="R5GadgetMat", x_min=-2.0, x_max=2.0, y_min=-2.0, y_max=2.0)
        m_gp.set_data([[float(r + c) for c in range(16)] for r in range(16)])
        g_gp = m_gp.plot_heatmap(graph_name="R5GadgetHeatmap")
        g_gp.attach_gadget("image_profile")
        ip_set = g_gp.set_gadget_roi(x=0.5, y=-0.5)
        assert ip_set.get("ok"), f"g.set_gadget_roi(x=0.5, y=-0.5) failed: {ip_set}"

        # ------------------------------------------------------------------
        # 3. #195 [P1-R4-1]: Swift K*(eps0+x)^n Auto-Guess (R^2 > 0.999) +
        #    GSL NaN / Out-of-Domain Jacobian Safety Guard
        # ------------------------------------------------------------------
        for sw_idx, (k_true, eps_true, n_true) in enumerate([(780.0, 0.015, 0.24), (520.0, 0.08, 0.18)]):
            xs_sw = [0.002 + 0.398 * i / 69.0 for i in range(70)]
            ys_sw = [k_true * ((eps_true + x) ** n_true) for x in xs_sw]
            t_sw = app.create_table(rows=70, cols=2, col_names=["x", "y"], name=f"SwiftR5_{sw_idx}")
            t_sw.set_data_dict({"x": xs_sw, "y": ys_sw})
            res_sw = app.fit_nonlinear(table=t_sw, x_col="x", y_col="y", model="Swift")
            assert res_sw.get("ok") and res_sw.get("converged") and float(res_sw.get("r2", 0.0)) > 0.9999, (
                f"Swift auto-guess failed on regime {sw_idx}: {res_sw}"
            )
            p_sw = res_sw["params"]
            assert (
                abs(float(p_sw["K"]) - k_true) < 1.0
                and abs(float(p_sw["eps0"]) - eps_true) < 1e-3
                and abs(float(p_sw["n"]) - n_true) < 1e-3
            ), f"Swift parameter recovery mismatch: {p_sw}"

        # Out-of-domain custom Jacobian guard: sqrt(x - c) initialized at c = 100 on x in [1..10]
        # must NEVER falsely report converged=True at R^2 < 0.99 due to NaN Jacobian!
        t_dom = app.create_table(rows=10, cols=2, col_names=["x", "y"], name="DomGuardTbl")
        xs_dom = [float(i + 1) for i in range(10)]
        ys_dom = [3.0 * math.sqrt(x - 0.5) for x in xs_dom]
        t_dom.set_data_dict({"x": xs_dom, "y": ys_dom})
        res_dom = app.fit_nonlinear(
            table=t_dom,
            x_col="x",
            y_col="y",
            formula="A * sqrt(x - c)",
            param_names=["A", "c"],
            initial_values={"A": 2.0, "c": 100.0},
        )
        if res_dom.get("converged"):
            assert float(res_dom.get("r2", 0.0)) > 0.999, (
                f"Out-of-domain Jacobian falsely reported converged=True on bad fit: {res_dom}"
            )
        else:
            assert res_dom.get("ok") is False, f"Non-converged domain error returned ok=True: {res_dom}"

        # ------------------------------------------------------------------
        # 4. #195 [P1-R4-2] + #196 [P2-R4-1]: Unified 2-Level Text Binary
        #    Encoding Across fit_logistic and stats.roc
        # ------------------------------------------------------------------
        xs_bin = [-5.0 + 0.5 * i for i in range(21)]
        # Verify qa-shard-1 exact repro pairs (including reversed ("case", "healthy")) + JP & Disease pairs
        for idx_b, (nm, lo_lbl, hi_lbl, exp_roc_auc) in enumerate(
            [
                ("ctrl_trt", "control", "treated", 1.0),
                ("case_hlth", "case", "healthy", 0.0),  # "case" is positive, at x <= 0 -> logistic AUC=1.0, raw-score ROC AUC=0.0
                ("jp_pair", "陰性", "陽性", 1.0),
                ("hlth_dis", "Healthy", "Disease", 1.0),
                ("ctrl_dis", "Control", "Disease", 1.0),
                ("none_evt", "none", "event", 1.0),
            ]
        ):
            t_b = app.create_table(rows=21, cols=2, col_names=["X", "Y"], name=f"BinTxt_{idx_b}_{nm}")
            t_b.set_column("X", xs_bin)
            t_b.set_column("Y", [hi_lbl if x > 0.0 else lo_lbl for x in xs_bin], mode="text")

            res_log = app.fit_logistic(table=t_b, y_col="Y", x_cols=["X"], plot=False)
            assert res_log.get("ok") and abs(float(res_log.get("auc", 0.0)) - 1.0) < 1e-6, (
                f"fit_logistic failed 2-level text pair ({lo_lbl!r}, {hi_lbl!r}): {res_log}"
            )

            res_roc = app.roc(table=t_b, state_col="Y", score_col="X")
            assert (
                res_roc.get("ok")
                and int(res_roc.get("n_pos", 0)) > 0
                and int(res_roc.get("n_neg", 0)) > 0
                and abs(float(res_roc.get("auc", -1.0)) - exp_roc_auc) < 1e-6
                and res_roc.get("graph")
            ), f"stats.roc failed 2-level text pair ({lo_lbl!r}, {hi_lbl!r}): {res_roc}"
            roc_ginfo = app.graph(res_roc["graph"]).info()
            assert roc_ginfo.get("all_fonts_arial") is True, (
                f"ROC graph lost Arial font on ({lo_lbl!r}, {hi_lbl!r}): {roc_ginfo}"
            )

        # ------------------------------------------------------------------
        # 5. #208 [P0-R5-1] + [P1-R5-1]: VectorCurve Empty/NaN/Masked Guard,
        #    Underscore VectorCurve + Rename + .sciprj Reload, CT_Strain_Raw
        #    Rename + Live Edit + Short Lookup + .sciprj Reload, Table1->ExpA
        #    & Matrix1->MatA & Column V->V_new Prefix Coexistence
        # ------------------------------------------------------------------
        app.new_project()
        # (5a) Empty, all-NaN, and all-masked plot_vectors -> RpcError(-32602), 0 leaked graphs
        t_ev = app.create_table(rows=4, cols=4, name="EmptyVecR5")
        wins_before_ev = len(app.list_windows())
        try:
            app.plot_vectors(t_ev, [0, 1, 2, 3])
            raise AssertionError("Empty table plot_vectors should raise RpcError(-32602)")
        except tg.RpcError as exc:
            assert exc.code == -32602, f"Expected -32602 on empty plot_vectors, got {exc.code}"
        assert len(app.list_windows()) == wins_before_ev, "Empty plot_vectors leaked a Graph window"

        t_ev.set_data_dict(
            {
                "X": [float("nan")] * 4,
                "Y": [float("nan")] * 4,
                "U": [float("nan")] * 4,
                "V": [float("nan")] * 4,
            }
        )
        try:
            app.plot_vectors(t_ev, ["X", "Y", "U", "V"])
            raise AssertionError("All-NaN table plot_vectors should raise RpcError(-32602)")
        except tg.RpcError as exc:
            assert exc.code == -32602, f"Expected -32602 on all-NaN plot_vectors, got {exc.code}"

        t_ev.set_data_dict({"X": [0, 1, 2, 3], "Y": [0, 1, 2, 3], "U": [1, 1, 1, 1], "V": [2, 2, 2, 2]})
        t_ev.mask_rows([0, 1, 2, 3])
        try:
            app.plot_vectors(t_ev, ["X", "Y", "U", "V"])
            raise AssertionError("All-masked table plot_vectors should raise RpcError(-32602)")
        except tg.RpcError as exc:
            assert exc.code == -32602, f"Expected -32602 on all-masked plot_vectors, got {exc.code}"

        # (5b) VectorCurve with underscore table & underscore cols (raw_U, raw_V) + rename_window + .sciprj reload
        app.new_project()
        tv_u = app.create_table(rows=4, cols=4, name="Vec_Under_Tbl")
        tv_u.set_data_dict({"X": [0, 1, 2, 3], "Y": [0, 1, 2, 3], "raw_U": [1, 1, 1, 1], "raw_V": [2, 2, 2, 2]})
        gv_u = app.plot_vectors(tv_u, ["X", "Y", "raw_U", "raw_V"], graph_name="VecGraphR5")
        assert gv_u.info()["layers"][0]["curve_count"] == 1
        ren_vec = app.rename_window(tv_u, "Exp_Vec_Tbl")
        assert ren_vec.get("name") == "Exp-Vec-Tbl" and ren_vec.get("new_name") == "Exp-Vec-Tbl", ren_vec
        assert tv_u.name == "Exp-Vec-Tbl"
        vec_sciprj = tmp_dir / "vec_under_r5.sciprj"
        app.save_project(vec_sciprj)
        app.open_project(vec_sciprj)
        gv_u_reload = app.window_info("VecGraphR5")
        assert gv_u_reload["layers"][0]["curve_count"] == 1, f"VectorCurve lost on reload: {gv_u_reload}"

        # (5c) Underscore table CT_Strain_Raw rename + live cell edit + short-curve lookup + .sciprj reload
        app.new_project()
        t_ct = app.create_table(rows=5, cols=2, name="CT_Strain_Raw")
        t_ct.set_data_dict({"Time": [0.0, 1.0, 2.0, 3.0, 4.0], "V": [1.0, 3.0, 5.0, 7.0, 9.0]})
        g_ct = app.plot_xy(t_ct, "Time", "V", graph_name="StrainGraphR5")
        ren_ct = app.rename_window("CT_Strain_Raw", "Renamed_Strain_Tbl")
        assert ren_ct.get("new_name") == "Renamed-Strain-Tbl", ren_ct
        app.table("Renamed-Strain-Tbl").set_cell(4, 1, 999.0)
        assert abs(float(g_ct.info()["layers"][0]["curves"][0]["y_max"]) - 999.0) < 1e-6, (
            f"Live cell edit did not update curve after CT_Strain_Raw rename: {g_ct.info()}"
        )
        lkp_ct = app.lookup_xy(graph=g_ct, curve="V", x=4.0)
        assert lkp_ct.get("ok") and abs(float(lkp_ct["y"]) - 999.0) < 1e-6, (
            f"Short-curve lookup curve='V' failed after CT_Strain_Raw rename: {lkp_ct}"
        )
        ct_sciprj = tmp_dir / "ct_strain_r5.sciprj"
        app.save_project(ct_sciprj)
        app.open_project(ct_sciprj)
        g_ct_reload = app.window_info("StrainGraphR5")
        assert (
            g_ct_reload["layers"][0]["curve_count"] == 1
            and abs(float(g_ct_reload["layers"][0]["curves"][0]["y_max"]) - 999.0) < 1e-6
        ), f"CT_Strain_Raw curve lost on .sciprj reload: {g_ct_reload}"

        # (5d) Table1 -> ExpA coexistence with Table10 & Table1_Raw (2D + Graph3D + Legend)
        app.new_project()
        t1 = app.create_table(rows=4, cols=3, name="Table1")
        t10 = app.create_table(rows=4, cols=3, name="Table10")
        t1_raw = app.create_table(rows=4, cols=3, name="Table1_Raw")
        t1.set_data_dict({"X": [0, 1, 0, 1], "Y": [10, 11, 12, 13], "Z": [10, 11, 12, 13]})
        t10.set_data_dict({"X": [0, 1, 0, 1], "Y": [100, 101, 102, 103], "Z": [100, 101, 102, 103]})
        t1_raw.set_data_dict({"X": [0, 1, 0, 1], "Y": [1000, 1001, 1002, 1003], "Z": [1000, 1001, 1002, 1003]})
        g_coex_t = app.plot_xy(t1, "X", "Y", graph_name="CoexTbl2D")
        g_coex_t.add_curve(t10, "Y", x_col="X")
        g_coex_t.add_curve(t1_raw, "Y", x_col="X")
        g_coex_t.set_legend(
            visible=True,
            text="\\c{1}Table1_Y\n\\c{2}Table10_Y\n\\c{3}Table1_Raw_Y\n",
        )
        app.plot_3d_xyz(t1, "X", "Y", "Z", graph_name="Coex3DT1")
        app.plot_3d_xyz(t10, "X", "Y", "Z", graph_name="Coex3DT10")
        app.rename_window("Table1", "ExpA")
        info_coex_t = g_coex_t.info()["layers"][0]
        titles_coex_t = [c["title"] for c in info_coex_t["curves"]]
        assert titles_coex_t == ["ExpA_Y", "Table10_Y", "Table1_Raw_Y"], f"Curve titles corrupted: {titles_coex_t}"
        leg_coex_t = info_coex_t.get("legend_text", "")
        assert (
            "ExpA_Y" in leg_coex_t
            and "Table10_Y" in leg_coex_t
            and "Table1_Raw_Y" in leg_coex_t
            and "ExpA0_Y" not in leg_coex_t
            and "ExpA_Raw_Y" not in leg_coex_t
        ), f"Legend corrupted by Table1->ExpA rename: {leg_coex_t!r}"
        coex_t_sciprj = tmp_dir / "coex_tbl_r5.sciprj"
        app.save_project(coex_t_sciprj)
        app.open_project(coex_t_sciprj)
        assert len(app.list_windows("Graph3D")) == 2, "Table1->ExpA destroyed Table10 Graph3D on reload!"
        assert app.window_info("CoexTbl2D")["layers"][0]["curve_count"] == 3

        # (5e) Matrix1 -> MatA coexistence with Matrix10 (Spectrogram-only 2D graph + Graph3D)
        app.new_project()
        m1 = app.create_matrix(rows=4, cols=4, name="Matrix1")
        m10 = app.create_matrix(rows=4, cols=4, name="Matrix10")
        m1.set_data([[float(r + c + 1) for c in range(4)] for r in range(4)])
        m10.set_data([[float(10 * (r + c + 1)) for c in range(4)] for r in range(4)])
        m1.plot_heatmap(graph_name="CoexHMap1")
        m10.plot_heatmap(graph_name="CoexHMap10")
        app.plot_3d_matrix(m1, style="surface", graph_name="CoexM3D1")
        app.plot_3d_matrix(m10, style="surface", graph_name="CoexM3D10")
        app.rename_window("Matrix1", "MatA")
        coex_m_sciprj = tmp_dir / "coex_mtx_r5.sciprj"
        app.save_project(coex_m_sciprj)
        app.open_project(coex_m_sciprj)
        assert len(app.list_windows("Graph3D")) == 2, "Matrix1->MatA destroyed Matrix10 Graph3D on reload!"
        assert app.window_info("CoexHMap1")["layers"][0]["spectrograms"][0]["matrix"] == "MatA"
        assert app.window_info("CoexHMap10")["layers"][0]["spectrograms"][0]["matrix"] == "Matrix10"

        # (5f) Column rename V -> V_new coexistence with V10
        app.new_project()
        t_cr = app.create_table(rows=4, cols=4, col_names=["X", "V", "V10", "Z"], name="ColRenTbl")
        t_cr.set_data_dict(
            {
                "X": [0.0, 1.0, 2.0, 3.0],
                "V": [1.0, 3.0, 5.0, 7.0],
                "V10": [10.0, 30.0, 50.0, 70.0],
                "Z": [2.0, 4.0, 6.0, 8.0],
            }
        )
        g_cr = app.plot_xy(t_cr, "X", ["V", "V10"], graph_name="ColRen2D")
        app.plot_3d_xyz(t_cr, "X", "V10", "Z", graph_name="ColRen3D")
        t_cr.set_meta("V", name="V_new")
        cr_layer = g_cr.info()["layers"][0]
        assert [c["title"] for c in cr_layer["curves"]] == ["ColRenTbl_V_new", "ColRenTbl_V10"], cr_layer
        cr_leg = cr_layer.get("legend_text", "")
        assert "\\c{1}V_new" in cr_leg and "\\c{2}V10" in cr_leg and "V_new10" not in cr_leg, cr_leg
        g_cr.set_legend(visible=True, text="\\c{1}ColRenTbl_V_new\n\\c{2}ColRenTbl_V10\n")
        t_cr.set_meta("V_new", name="V_final")
        cr_leg2 = g_cr.info()["layers"][0].get("legend_text", "")
        assert (
            "\\c{1}ColRenTbl_V_final" in cr_leg2
            and "\\c{2}ColRenTbl_V10" in cr_leg2
            and "V_final10" not in cr_leg2
        ), cr_leg2
        col_ren_sciprj = tmp_dir / "col_ren_r5.sciprj"
        app.save_project(col_ren_sciprj)
        app.open_project(col_ren_sciprj)
        assert len(app.list_windows("Graph3D")) == 1, "Column rename V->V_new destroyed V10 Graph3D on reload!"

    runner.check(
        "11. Round 5 Zero-Defect Gate (`raw_V` vs `V` Multi-Curve, `set_gadget_roi` Kwargs, `Swift` Auto-Guess & Domain Jacobian, 2-Level Text `fit_logistic`/`roc`)",
        _w11_round5_zero_defect_gate,
    )


def run_workflow_12_phase_b_api_sdk_gate(
    runner: E2ERunner, app: tg.Session, tmp_dir: Path
) -> None:
    """Phase B Gate: API/SDK Bug Fixes, Self-Discovery, Layout Diagnostics, Lab CSV/TSV & Tensile 0.2% Proof Stress."""

    def _w12_self_discovery_and_errors() -> None:
        methods = app.supported_methods()
        cat = app.describe_api()
        assert cat.get("ok") is True and cat.get("count") == len(methods) and len(methods) >= 201, (
            f"describe_api count mismatch vs supported_methods ({len(methods)}): {cat.get('count')}"
        )
        cat_names = [m["method"] for m in cat["methods"]]
        assert set(cat_names) == set(methods), (
            f"describe_api parity mismatch: diff={set(cat_names) ^ set(methods)}"
        )

        g_dom = app.describe_api(domain="graph")
        assert g_dom.get("count", 0) >= 35 and all(
            m["domain"] == "graph" and m["method"].startswith("graph.") for m in g_dom["methods"]
        )
        insp_spec = app.describe_api("graph.inspect")
        assert (
            insp_spec.get("method") == "graph.inspect"
            and isinstance(insp_spec.get("params"), list)
            and insp_spec.get("count") == 1
            and isinstance(insp_spec.get("methods"), list)
            and len(insp_spec["methods"]) == 1
        ), f"Single-method describe_api shape mismatch: {insp_spec}"

        # [P1-R2-5] Verify 100% of advertised sdk_method / sdk entries exist on Session / Proxy classes
        cls_map = {
            "Session": tg.Session,
            "TableProxy": tg.TableProxy,
            "MatrixProxy": tg.MatrixProxy,
            "GraphProxy": tg.GraphProxy,
        }
        for entry in cat["methods"]:
            sdk_sig = str(entry.get("sdk_method") or entry.get("sdk") or "")
            if "." in sdk_sig:
                cls_name, attr_part = sdk_sig.split(".", 1)
                attr_name = attr_part.split("(", 1)[0].strip()
                if cls_name in cls_map and attr_name:
                    assert hasattr(cls_map[cls_name], attr_name), (
                        f"Catalog method {entry['method']} advertises missing {cls_name}.{attr_name}"
                    )
                    alias_of = getattr(getattr(cls_map[cls_name], attr_name), "_torigraph_alias_of", None)
                    assert alias_of is None, (
                        f"Catalog method {entry['method']} advertises deprecated alias "
                        f"{cls_name}.{attr_name} (use {alias_of})"
                    )

        # Actionable -32602 errors with `available` list
        app.new_project()
        t_err = app.create_table(rows=4, cols=2, col_names=["Strain", "Stress"], name="ErrCheckTbl")
        t_err.set_data_dict({"Strain": [0.0, 1.0, 2.0, 3.0], "Stress": [0.0, 10.0, 20.0, 30.0]})
        try:
            t_err.get_column("NonExistentCol")
            raise AssertionError("Expected RpcError(-32602) on missing column")
        except tg.RpcError as exc:
            assert exc.code == -32602 and exc.available is not None and "Strain" in exc.available, (
                f"Missing actionable available list on column lookup error: {exc.data}"
            )

        g_err = app.plot_xy(t_err, "Strain", "Stress", graph_name="ErrCheckGraph")
        for bad_call in (
            lambda: g_err.apply_palette("not_a_real_palette"),
            lambda: g_err.set_curve_style(0, ls="not_a_linestyle"),
            lambda: g_err.set_curve_style(0, marker="not_a_marker"),
            lambda: g_err.set_axis(layer=1, min=0.0, max=5.0),  # strict 0-based: layer=1 out of range on 1-layer graph
            lambda: app.close_window(""),
            lambda: app.window_info(""),
        ):
            caught_32602 = False
            try:
                bad_call()
            except tg.RpcError as exc:
                caught_32602 = exc.code == -32602
            assert caught_32602, "Expected strict RpcError(-32602) on invalid enum/layer/empty-name!"

        # [P0-API-1] Modal dialog blocking (actionMergeGraphs + ellipsis + Japanese modal actions)
        for modal_target in (
            "actionMergeGraphs",
            "mergeOpenGraphsDialog",
            "ウィンドウを印刷",
            "プロットウィザード...",
            "エクスポート...",
        ):
            blocked = False
            try:
                app.trigger_action(modal_target)
            except tg.RpcError as exc:
                blocked = exc.code == -32602
            assert blocked, f"Modal dialog target {modal_target!r} was not blocked with -32602!"

    runner.check("12.1 Self-Discovery (`describe_api`), Actionable `-32602` & Modal Dialog Guard", _w12_self_discovery_and_errors)

    def _w12_lab_csv_tsv_numeric_undo_gc() -> None:
        app.new_project()

        # [P2-API-1] Non-ASCII windowLabel + [P2-2] 'g'(6) default display format + [P0-API-2] set_cell NaN sentinels
        t_jp = app.create_table(rows=4, cols=2, col_names=["X", "Y"], name="引張試験データ")
        jp_winfo = app.window_info(t_jp)
        assert "引張試験データ" in str(jp_winfo.get("label", "")), f"Non-ASCII label lost: {jp_winfo}"
        meta0 = t_jp.get_meta(0)
        assert meta0.get("numeric_format") == "g" and int(meta0.get("precision", 0)) == 6, (
            f"Expected default 'g'(6) format on numeric column, got {meta0}"
        )

        t_jp.set_data_dict({"X": [1.0, 2.0, 3.0, 4.0], "Y": [10.0, 20.0, 30.0, 40.0]})
        for sentinel in ("", "NaN", "-inf", "null", "none", "N/A", "-"):
            t_jp.set_cell(1, "Y", sentinel)
            assert t_jp.info()["columns"][1]["mode"] == "numeric", (
                f"set_cell({sentinel!r}) flipped numeric column to text!"
            )
            assert t_jp.get_cell(1, "Y") is None, f"set_cell({sentinel!r}) did not mark cell invalid!"

        # [P0-API-4] Headless Table & Matrix undo/redo
        t_jp.set_cell(0, "Y", 10.0)
        t_jp.set_cell(0, "Y", 777.0)
        assert abs(float(t_jp.get_cell(0, "Y")) - 777.0) < 1e-9
        t_jp.undo()
        assert abs(float(t_jp.get_cell(0, "Y")) - 10.0) < 1e-9, "Table.undo() failed in headless mode!"
        t_jp.redo()
        assert abs(float(t_jp.get_cell(0, "Y")) - 777.0) < 1e-9, "Table.redo() failed in headless mode!"

        m_ur = app.create_matrix(rows=2, cols=2, name="UndoMat")
        m_ur.set_data([[1.0, 2.0], [3.0, 4.0]])
        m_ur.set_data([[9.0, 8.0], [7.0, 6.0]])
        m_ur.undo()
        assert abs(float(m_ur.get_data()[0][0]) - 1.0) < 1e-9, "Matrix.undo() failed in headless mode!"
        m_ur.redo()
        assert abs(float(m_ur.get_data()[0][0]) - 9.0) < 1e-9, "Matrix.redo() failed in headless mode!"

        # [P1-API-7] Matrix <-> Table NaN preservation & NaN determinant/invert rejection
        m_nan = app.create_matrix(rows=2, cols=2, name="NanMat")
        m_nan.set_data([[1.0, None], [3.0, 4.0]])
        for bad_mat_op in (m_nan.determinant, m_nan.invert):
            rejected_nan = False
            try:
                bad_mat_op()
            except tg.RpcError as exc:
                rejected_nan = exc.code == -32602
            assert rejected_nan, "Matrix determinant/invert failed to reject NaN with -32602!"
        t_from_m = m_nan.to_table(name="TblFromNanMat")
        assert t_from_m.get_cell(0, 1) is None, f"matrix.to_table lost NaN cell: {t_from_m.get_data_dict()}"
        m_back = t_from_m.to_matrix(name="MatBackFromNanTbl")
        assert m_back.get_data()[0][1] is None, f"table.to_matrix lost invalid cell as NaN: {m_back.get_data()}"

        # [P0-2] & [P2-2] Shift-JIS CSV + 2-row [Name, Units] Instron CSV + Rigaku .ras + paste_tsv
        sjis_csv = tmp_dir / "shimadzu_sjis.csv"
        sjis_text = (
            "# 試験機: 島津製作所 AG-X\n"
            "ひずみ (%),公称応力 (MPa)\n"
            "0.00,0.0\n"
            "0.10,205.5\n"
            "0.20,-\n"
            "0.30,465.2\n"
        )
        sjis_csv.write_bytes(sjis_text.encode("cp932"))
        t_sjis = app.import_csv(sjis_csv, encoding="auto", name="ShimadzuSJIS")
        sjis_cols = t_sjis.info()["columns"]
        assert sjis_cols[0]["mode"] == "numeric" and sjis_cols[1]["mode"] == "numeric", sjis_cols
        assert sjis_cols[0].get("units") == "%" and sjis_cols[1].get("units") == "MPa", sjis_cols
        assert t_sjis.get_cell(2, 1) is None and abs(float(t_sjis.get_cell(3, 1)) - 465.2) < 1e-6

        instron_csv = tmp_dir / "instron_2row.csv"
        instron_csv.write_text(
            "Extension,Load,Specimen_ID\n"
            "mm,N,\n"
            "0.0,0.0,SP-A1\n"
            "0.5,125.4,SP-A1\n"
            "1.0,250.8,SP-A1\n",
            encoding="utf-8",
        )
        t_ins = app.import_csv(instron_csv, name="Instron2Row")
        ins_info = t_ins.info()
        assert ins_info["rows"] == 3, f"2-row units CSV row count mismatch: {ins_info}"
        assert ins_info["columns"][0]["mode"] == "numeric" and ins_info["columns"][0]["units"] == "mm", ins_info
        assert ins_info["columns"][1]["mode"] == "numeric" and ins_info["columns"][1]["units"] == "N", ins_info
        assert ins_info["columns"][2]["mode"] == "text" and t_ins.get_cell(0, 2) == "SP-A1", ins_info

        ras_path = tmp_dir / "rigaku_xrd.ras"
        ras_path.write_text(
            "*RAS_DATA_START\n"
            "*RAS_HEADER_START\n"
            '*MEAS_SCAN_UNIT_X "deg"\n'
            '*MEAS_SCAN_UNIT_Y "cps"\n'
            "*RAS_HEADER_END\n"
            "*RAS_INT_START\n"
            "20.00 120.0 1.0\n"
            "20.05 340.0 1.0\n"
            "20.10 980.0 1.0\n"
            "*RAS_INT_END\n"
            "*RAS_DATA_END\n",
            encoding="utf-8",
        )
        t_ras = app.import_csv(ras_path, name="RigakuXRD")
        assert t_ras.info()["rows"] == 3 and abs(float(t_ras.get_cell(2, 1)) - 980.0) < 1e-6

        t_pasted = app.paste_tsv(
            "Temperature / K\tRate [s^-1]\n300\t1.25e-4\n350\t8.50e-4\n",
            name="PastedKineticTbl",
        )
        p_cols = t_pasted.info()["columns"]
        assert p_cols[0]["units"] == "K" and p_cols[1]["units"] == "s^-1" and t_pasted.info()["rows"] == 2

        # [P1-R2-3] Sub-block paste_tsv(start_row > 0) preserves first pasted row & paste_tsv handles *RAS_ text
        t_sub = app.create_table(rows=2, cols=2, col_names=["Time", "Temp"], name="SubBlockPasteTbl")
        t_sub.set_column("Time", [0.0, 1.0])
        t_sub.set_column("Temp", [300.0, 310.0])
        t_sub.paste_tsv("2.0\t320.0\n3.0\t330.0\n", start_row=2, start_col=0)
        assert t_sub.get_column("Time") == [0.0, 1.0, 2.0, 3.0], f"Sub-block paste dropped row 0: {t_sub.get_data_dict()}"
        assert t_sub.get_column("Temp") == [300.0, 310.0, 320.0, 330.0]

        t_ras_paste = app.paste_tsv(ras_path.read_text(encoding="utf-8"), name="PastedRigakuRAS")
        assert (
            t_ras_paste.info()["rows"] == 3
            and t_ras_paste.info()["columns"][0]["mode"] == "numeric"
            and abs(float(t_ras_paste.get_cell(2, 1)) - 980.0) < 1e-6
        ), f"paste_tsv failed on Rigaku .ras text: {t_ras_paste.info()}"

        # [P0-API-6] weakref.finalize cleanup on unclosed Session
        assert app._finalizer is not None and app._finalizer.alive is True
        sub_sess = tg.launch(headless=True)
        sub_proc = sub_sess.process
        sub_files = list(sub_sess._cleanup_files)
        assert sub_proc is not None and sub_proc.poll() is None
        del sub_sess
        gc.collect()
        deadline = time.monotonic() + 6.0
        while sub_proc.poll() is None and time.monotonic() < deadline:
            time.sleep(0.05)
        assert sub_proc.poll() is not None, "weakref.finalize did not terminate leaked ToriGraph process!"
        assert all(not os.path.exists(fp) for fp in sub_files), f"Temp files leaked after GC: {sub_files}"

    runner.check("12.2 Lab CSV/TSV/RAS Import, Sentinels, Headless Undo/Redo & GC Cleanup", _w12_lab_csv_tsv_numeric_undo_gc)

    def _w12_inspect_diagnose_kwargs_lifecycle_tensile() -> None:
        app.new_project()

        # [P2-API-2] All-NaN plot rejection without leaking windows
        t_allnan = app.create_table(rows=4, cols=2, col_names=["X", "Y"], name="AllNanPlotTbl")
        t_allnan.set_data_dict({"X": [None, None, None, None], "Y": [None, None, None, None]})
        wins0 = len(app.list_windows())
        for fn_nan_plot in (
            lambda: app.plot_xy(t_allnan, "X", "Y"),
            lambda: app.plot_box(t_allnan, ["Y"]),
            lambda: app.plot_pie(t_allnan, "Y"),
        ):
            got_err = False
            try:
                fn_nan_plot()
            except tg.RpcError as exc:
                got_err = exc.code == -32602
            assert got_err, "All-NaN plot should raise RpcError(-32602)!"
        assert len(app.list_windows()) == wins0, "All-NaN plot leaked empty Graph window!"

        # Universal matplotlib kwargs + symbol_skip + inspect() + layout diagnostics
        xs = [float(i) * 0.5 for i in range(1, 25)]
        ys = [2.0 * x + 5.0 for x in xs]
        t_kw = app.create_table(rows=len(xs), cols=2, col_names=["Time", "Signal"], name="KwargInspectTbl")
        t_kw.set_data_dict({"Time": xs, "Signal": ys})
        g_kw = app.plot_xy(
            t_kw,
            "Time",
            "Signal",
            style="line_symbol",
            lw=2.0,
            ls="--",
            marker="s",
            ms=7,
            symbol_skip=3,
            color="#D55E00",
            xlabel="Time (s)",
            ylabel="Signal (mV)",
            xlim=[0.0, 13.0],
            ylim=[0.0, 35.0],
            title="Initial Title",
            graph_name="KwargInspectGraph",
        )
        g_kw.apply_series_scheme("okabe_ito")
        g_kw.set_curve_style(0, symbol_skip=3, lw=2.0, ls="--", marker="s", ms=7)

        snap = g_kw.inspect()
        assert snap.get("ok") is True and "page" in snap and "grid" in snap
        lyr0 = snap["layers"][0]
        assert len(lyr0["canvas_rect_px"]) == 4 and lyr0["canvas_rect_px"][2] > 20 and lyr0["canvas_rect_px"][3] > 20
        assert lyr0["curves"][0]["symbol_skip"] == 3 and lyr0["curves"][0]["valid_points"] == len(xs)
        assert len(lyr0["axes"]["x"]["major_ticks"]) >= 2

        # Trigger deterministic layout warnings: CURVE_CLIPPED_BY_AXIS + DISABLED_AXIS_HAS_TITLE_OR_CURVE
        g_kw.set_axis("y", lim=[100.0, 200.0])  # clips 100% of [7..29] data
        g_kw.set_axis("right", title="Orphan Right Title", enabled=False)
        warn_codes = {w["code"] for w in g_kw.diagnose_layout()}
        assert "CURVE_CLIPPED_BY_AXIS" in warn_codes, f"Expected CURVE_CLIPPED_BY_AXIS in {warn_codes}"
        assert "DISABLED_AXIS_HAS_TITLE_OR_CURVE" in warn_codes, (
            f"Expected DISABLED_AXIS_HAS_TITLE_OR_CURVE in {warn_codes}"
        )
        # Fix both layout issues and verify those warnings clear
        g_kw.set_axis("y", lim=[0.0, 35.0], show_labels=True, show_ticks=True, precision=1)
        g_kw.set_axis("right", title="", enabled=False)
        warn_codes_after = {w["code"] for w in g_kw.diagnose_layout()}
        assert "CURVE_CLIPPED_BY_AXIS" not in warn_codes_after
        assert "DISABLED_AXIS_HAS_TITLE_OR_CURVE" not in warn_codes_after

        # Trigger NONPOSITIVE_DATA_ON_LOG_AXIS
        t_neg = app.create_table(rows=4, cols=2, col_names=["X", "Y"], name="LogWarnTbl")
        t_neg.set_data_dict({"X": [-2.0, -1.0, 1.0, 10.0], "Y": [1.0, 2.0, 3.0, 4.0]})
        g_log = app.plot_xy(t_neg, "X", "Y")
        g_log.set_axis("x", min=0.1, max=20.0, scale="log10")
        assert "NONPOSITIVE_DATA_ON_LOG_AXIS" in {w["code"] for w in g_log.diagnose_layout()}

        # English fit label hygiene ([P0-4]) + "Gaussian" alias on fit_nonlinear
        fit_lin = app.fit_linear(graph=g_kw, fit_label="Linear Calibration")
        assert fit_lin.get("ok") is True
        kw_titles = [c["title"] for c in g_kw.inspect()["layers"][0]["curves"]]
        assert any("Linear Calibration" in ct for ct in kw_titles), f"fit_label not applied: {kw_titles}"

        xs_g = [-4.0 + 0.25 * i for i in range(33)]
        ys_g = [1.0 + 15.0 * math.exp(-((x - 0.5) / 1.1) ** 2) for x in xs_g]
        t_gau = app.create_table(rows=len(xs_g), cols=2, col_names=["X", "Y"], name="GaussianAliasTbl")
        t_gau.set_data_dict({"X": xs_g, "Y": ys_g})
        res_gau = app.fit_nonlinear(table=t_gau, x_col="X", y_col="Y", model="Gaussian")
        assert res_gau.get("converged") is True and float(res_gau.get("r2", 0.0)) > 0.999

        # Lifecycle RPCs: set_title, add_text/add_arrow + remove_marker, reference lines, inset, linked top axis, remove_layer, merge_graphs
        g_kw.set_title("")
        assert g_kw.inspect()["layers"][0]["title"] == ""
        m_txt = g_kw.add_text("MarkerNote", x=2.0, y=10.0)
        m_arr = g_kw.add_arrow(1.0, 5.0, 3.0, 15.0)
        assert len(g_kw.inspect()["layers"][0]["text_markers"]) >= 1
        assert len(g_kw.inspect()["layers"][0]["arrows"]) >= 1
        g_kw.remove_marker(m_txt.get("id"))
        g_kw.remove_marker(kind="arrow", index=0)
        assert len(g_kw.inspect()["layers"][0]["text_markers"]) == 0
        assert len(g_kw.inspect()["layers"][0]["arrows"]) == 0

        g_kw.add_reference_line(value=15.0, orientation="horizontal", label="Ref15")
        g_kw.add_reference_line(value=25.0, orientation="horizontal", label="Ref25")
        assert len(g_kw.inspect()["layers"][0]["reference_lines"]) == 2
        g_kw.remove_reference_line(0)
        assert len(g_kw.inspect()["layers"][0]["reference_lines"]) == 1
        g_kw.clear_reference_lines()
        assert len(g_kw.inspect()["layers"][0]["reference_lines"]) == 0

        ins_res = g_kw.add_inset(1.0, 4.0, 5.0, 15.0, show_connectors=False)
        assert ins_res.get("inset_layer") == 1 and g_kw.inspect()["active_layer"] == 0
        assert g_kw.inspect()["layers"][1]["inset_spec"]["show_connectors"] is False
        g_kw.remove_layer(1)
        assert g_kw.inspect()["layer_count"] == 1

        # Linked overlay top-axis routing + merge_graphs layers array & <ToriLinkedLayer> preservation
        g_lnk = app.plot_xy(t_kw, "Time", "Signal", graph_name="LinkedOverlaySrc")
        g_lnk.add_linked_layer(link_x=True, right_y=False)
        routed = g_lnk.set_axis("top", title="Linked Top Axis", enabled=True)
        assert routed.get("layer") == 1
        raw_merge = app.call("graph.merge_graphs", graphs=[g_kw.name, g_lnk.name], rows=1, cols=2)
        assert isinstance(raw_merge.get("layers"), list) and len(raw_merge["layers"]) == 3, (
            f"merge_graphs 'layers' must be QJsonArray list, got {type(raw_merge.get('layers'))}: {raw_merge}"
        )

        # drop_columns default layer=None on 1-layer graph + integrate_curve(plot_curve=False/True) + differentiate_curve
        app.drop_columns(t_kw, ["Signal"], x_column="Time", graph=g_kw)
        c_before_int = len(g_kw.info()["layers"][0]["curves"])
        int_no_plot = app.integrate_curve(g_kw, 0, plot_curve=False)
        assert int_no_plot.get("ok") is True and len(g_kw.info()["layers"][0]["curves"]) == c_before_int
        int_with_plot = app.integrate_curve(g_kw, 0, plot_curve=True)
        assert int_with_plot.get("ok") is True and int_with_plot.get("output_curve")
        diff_res = app.differentiate_curve(graph=g_kw, curve=0)
        assert diff_res.get("ok") and diff_res.get("graph") and diff_res.get("table") and diff_res.get("output_curve")

        # Zero-variance ANOVA & flat-signal analyze_peaks guards ([P1-API-5])
        t_zv = app.create_table(rows=4, cols=2, col_names=["G1", "G2"], name="ZeroVarAnovaTbl")
        t_zv.set_data_dict({"G1": [10.0, 10.0, 10.0, 10.0], "G2": [20.0, 20.0, 20.0, 20.0]})
        zv_aov = app.anova_oneway(table=t_zv, cols=["G1", "G2"])
        assert zv_aov.get("ok") is True and zv_aov.get("p_value") == 0.0 and zv_aov.get("f_statistic") is None, zv_aov

        t_flat = app.create_table(rows=20, cols=2, col_names=["X", "Y"], name="FlatSignalTbl")
        t_flat.set_data_dict({"X": [float(i) for i in range(20)], "Y": [5.0 for _ in range(20)]})
        flat_pk = app.analyze_peaks(table=t_flat, x_col="X", y_col="Y", num_peaks=2)
        assert flat_pk.get("ok") is False and flat_pk.get("peak_count") == 0, flat_pk

        # Materials Science Tensile 0.2% Offset Proof Stress (`g.analyze_tensile`)
        # Exact piecewise elastic + plastic strain: eps = sig/E + eps_p, so at eps_p = 0.002, sigma_0.2 == 450.0 MPa!
        e_true = 200000.0
        eps_vals: List[float] = []
        sig_vals: List[float] = []
        for i in range(41):  # pure linear elastic 0 .. 400 MPa (R^2 = 1.0)
            s = 10.0 * i
            sig_vals.append(s)
            eps_vals.append(s / e_true)
        for i in range(1, 81):  # work hardening 402.5 .. 600 MPa; at sig=450, eps_p=0.002 exactly!
            s = 400.0 + 2.5 * i
            eps_p = 0.002 * (((s - 400.0) / 50.0) ** 2.5)
            sig_vals.append(s)
            eps_vals.append(s / e_true + eps_p)
        t_ten = app.create_table(rows=len(eps_vals), cols=2, col_names=["Strain", "Stress"], name="Tensile02Tbl")
        t_ten.set_data_dict(
            {"Strain": eps_vals, "Stress": sig_vals},
            units={"Strain": "-", "Stress": "MPa"},
            long_names={"Strain": "Engineering Strain", "Stress": "Engineering Stress"},
        )
        g_ten = app.plot_xy(t_ten, "Strain", "Stress", style="line", graph_name="Tensile02Graph")
        ten_res = g_ten.analyze_tensile(
            offset=0.002,
            strain_in_percent=False,
            plot_construction=True,
            add_callout=True,
            create_true_stress_table=True,
        )
        assert ten_res.get("ok") is True, f"analyze_tensile failed: {ten_res}"
        assert abs(float(ten_res["youngs_modulus"]) - e_true) / e_true < 0.03, f"E mismatch: {ten_res}"
        assert abs(float(ten_res["youngs_modulus_gpa"]) - 200.0) < 6.0, f"E_GPa mismatch: {ten_res}"
        assert abs(float(ten_res["offset_yield_stress"]) - 450.0) < 6.0, f"sigma_0.2 mismatch: {ten_res}"
        assert float(ten_res["uts"]) > 450.0 and ten_res.get("true_stress_table"), ten_res
        assert_graph_arial(app, g_ten.name, "Tensile 0.2% Proof Stress Graph")

        # [P1-R2-2] GPa stress unit scaling in analyze_tensile (`youngs_modulus_gpa` ~ 200.0, not 0.2)
        t_gpa = app.create_table(rows=len(eps_vals), cols=2, col_names=["Strain", "Stress"], name="TensileGpaTbl")
        t_gpa.set_data_dict(
            {"Strain": eps_vals, "Stress": [s / 1000.0 for s in sig_vals]},
            units={"Strain": "-", "Stress": "GPa"},
        )
        g_gpa = app.plot_xy(t_gpa, "Strain", "Stress", style="line", graph_name="TensileGpaGraph")
        gpa_res = g_gpa.analyze_tensile(offset=0.002, strain_in_percent=False)
        assert abs(float(gpa_res["youngs_modulus"]) - 200.0) < 6.0 and abs(float(gpa_res["youngs_modulus_gpa"]) - 200.0) < 6.0, (
            f"GPa unit scaling wrong in analyze_tensile: {gpa_res}"
        )

        # [P1-R2-1] Brittle fracture linear-elastic specimen with sudden load drop must reject 0.2% proof stress (-32602)
        t_brit = app.create_table(rows=8, cols=2, col_names=["StrainPct", "StressMPa"], name="BrittleCeramicTbl")
        t_brit.set_data_dict(
            {
                "StrainPct": [0.00, 0.05, 0.10, 0.15, 0.20, 0.25, 0.30, 0.31],
                "StressMPa": [0.0, 150.0, 300.0, 450.0, 600.0, 750.0, 900.0, 0.0],
            },
            units={"StrainPct": "%", "StressMPa": "MPa"},
        )
        g_brit = app.plot_xy(t_brit, "StrainPct", "StressMPa", style="line")
        brittle_rejected = False
        try:
            g_brit.analyze_tensile(offset=0.002, strain_in_percent=True)
        except tg.RpcError as exc:
            brittle_rejected = exc.code == -32602
        assert brittle_rejected, "Brittle elastic fracture drop falsely produced a 0.2% proof stress!"

        # [P1-R2-4] & [P2-MATSCI-R2-3] Masked DataCurve -> valid_points==0 + EMPTY_CURVE; apply_series_scheme(**kwargs) + inward show_ticks
        t_msk = app.create_table(rows=4, cols=2, col_names=["X", "Y"], name="MaskedInspectTbl")
        t_msk.set_data_dict({"X": [0.0, 1.0, 2.0, 3.0], "Y": [10.0, 20.0, 30.0, 40.0]})
        g_msk = app.plot_xy(t_msk, "X", "Y", graph_name="MaskedInspectGraph")
        g_msk.apply_series_scheme("okabe_ito", line_width=2.5)
        g_msk.isij_style()
        insp_inward = g_msk.inspect()["layers"][0]["axes"]
        assert insp_inward["top"]["show_ticks"] is True and insp_inward["right"]["show_ticks"] is True, (
            f"Inward top/right ticks reported show_ticks=False: {insp_inward}"
        )
        t_msk.mask_rows([0, 1, 2, 3], col="Y")
        insp_masked = g_msk.inspect()
        assert insp_masked["layers"][0]["curves"][0]["valid_points"] == 0, insp_masked["layers"][0]["curves"][0]
        assert "EMPTY_CURVE" in set(insp_masked.get("warning_codes", [])), insp_masked.get("warnings")

    runner.check(
        "12.3 `graph.inspect` / `diagnose_layout`, Universal Kwargs, Lifecycle RPCs & Tensile `0.2%` Proof Stress",
        _w12_inspect_diagnose_kwargs_lifecycle_tensile,
    )


def run_workflow_13_megawave1_sdk_pptx_matsci_gate(
    runner: E2ERunner, app: tg.Session, tmp_dir: Path
) -> None:
    """Mega-Wave 1 Gate: Atomic Batch, Auto-Layout, Reproducible Python Script, NumPy/DataFrame Adapter, Zero-COM PowerPoint Native OLE Deck (`to_pptx`), Sample Gallery, Category/100% Bars/Polar, Kissinger TDS, Kocks-Mecking, XRD Williamson-Hall & VASP DOS/Bands."""

    def _w13_1_batch_autolayout_and_to_python() -> None:
        app.new_project()

        # 1. Direct list `app.batch` with `$prev` and `$steps[k]` variable piping
        direct_res = app.batch(
            [
                {"method": "table.create", "params": {"rows": 6, "cols": 2, "name": "W13_BatchDirectTbl"}},
                {
                    "method": "table.set_data_dict",
                    "params": {
                        "table": "$prev.name",
                        "data": {"X": [0.0, 1.0, 2.0, 3.0, 4.0, 5.0], "Y": [0.0, 12.0, 24.0, 36.0, 48.0, 60.0]},
                    },
                },
                {
                    "method": "plot.xy",
                    "params": {
                        "table": "$steps[0].name",
                        "x_col": "X",
                        "y_cols": ["Y"],
                        "graph_name": "W13-BatchDirectGraph",
                    },
                },
            ],
            stop_on_error=True,
            atomic=True,
            defer_replot=True,
        )
        assert direct_res.get("ok") is True and int(direct_res.get("completed", 0)) == 3, direct_res
        assert app.graph("W13-BatchDirectGraph").info()["layers"][0]["curve_count"] == 1

        # 2. Recording context manager `with app.batch() as b:` resolving unnamed proxy tokens
        with app.batch(atomic=True, defer_replot=True) as b:
            t_ctx = app.create_table(rows=8, cols=2)
            t_ctx.set_data_dict(
                {
                    "Strain": [0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0],
                    "Stress": [0.0, 120.0, 240.0, 340.0, 405.0, 445.0, 470.0, 485.0],
                },
                units={"Strain": "%", "Stress": "MPa"},
                long_names={"Strain": "Engineering strain", "Stress": "Engineering stress"},
            )
            g_ctx = app.plot_xy(t_ctx, "Strain", "Stress", style="line_symbol")
            g_ctx.isij_style()
            g_ctx.set_axis("x", lim=[0.0, 7.5])

        assert b.ok is True and len(b.commands) == 5, b.result
        assert not t_ctx.name.startswith("$steps[") and not g_ctx.name.startswith("$steps["), (
            f"Proxy names not resolved after batch flush: t={t_ctx.name}, g={g_ctx.name}"
        )
        assert g_ctx.info()["layers"][0]["curve_count"] == 1

        # 3. Atomic rollback on batch step failure
        wins_before_fail = {w["name"] for w in app.list_windows()}
        failed_properly = False
        try:
            with app.batch(atomic=True, stop_on_error=True) as b_fail:
                app.create_table(rows=3, cols=2, name="W13_ShouldRollbackTbl")
                app.call("nonexistent.invalid_rpc_method")
        except tg.RpcError:
            failed_properly = True
        assert failed_properly, "Failing atomic batch context did not raise RpcError!"
        wins_after_fail = {w["name"] for w in app.list_windows()}
        assert "W13_ShouldRollbackTbl" not in wins_after_fail and wins_after_fail == wins_before_fail, (
            f"Atomic batch failed to roll back workspace! before={wins_before_fail}, after={wins_after_fail}"
        )

        # 4. Self-Healing Layout Engine (`g.auto_layout` / `g.fix_layout`)
        g_ctx.set_axis("y", lim=[600.0, 800.0])  # clips curve completely
        g_ctx.set_axis("right", title="OrphanRightTitle", enabled=False)
        pre_codes = {w["code"] for w in g_ctx.diagnose_layout()}
        assert "CURVE_CLIPPED_BY_AXIS" in pre_codes and "DISABLED_AXIS_HAS_TITLE_OR_CURVE" in pre_codes, pre_codes

        heal = g_ctx.auto_layout(pad_fraction=0.05, allow_outside_legend=True)
        assert heal.get("ok") is True, f"auto_layout failed: {heal}"
        post_codes = {w["code"] for w in g_ctx.diagnose_layout()}
        assert "CURVE_CLIPPED_BY_AXIS" not in post_codes, f"Clipping persisted after auto_layout: {post_codes}"
        assert "DISABLED_AXIS_HAS_TITLE_OR_CURVE" not in post_codes, (
            f"Disabled axis warning persisted after auto_layout: {post_codes}"
        )

        # 5. Reproducible Code Generation (`g.to_python_script`) live `exec` round-trip
        g_ctx.add_reference_line(value=405.0, orientation="horizontal", label="Proof")
        g_ctx.add_text("UTS Region", x=5.0, y=460.0)
        py_out = tmp_dir / "w13_repro_standalone.py"
        standalone_src = g_ctx.to_python_script(output_path=py_out, standalone=True, embed_data="inline")
        assert py_out.is_file() and len(standalone_src.strip()) > 20
        compile(standalone_src, str(py_out), "exec")

        console_src = g_ctx.to_python_script(standalone=False, embed_data="inline")
        compile(console_src, "<to_python_inline>", "exec")
        app.new_project()
        assert len(app.list_windows()) == 0
        exec_globals: Dict[str, Any] = {"__builtins__": __builtins__, "app": app, "tg": tg}
        exec(console_src, exec_globals)
        rebuilt_mls = [w["name"] for w in app.list_windows() if w.get("type") == "MultiLayer"]
        assert len(rebuilt_mls) >= 1, f"Executed to_python_script produced no MultiLayer window:\n{console_src}"
        g_reb = app.graph(rebuilt_mls[0])
        assert g_reb.info()["layers"][0]["curve_count"] >= 1
        assert_graph_arial(app, g_reb.name, "W13 to_python_script Rebuilt")

    runner.check(
        "13.1 Atomic `app.batch` (`with app.batch()` + Rollback), `auto_layout` & `to_python_script` Exec Round-Trip",
        _w13_1_batch_autolayout_and_to_python,
    )

    def _w13_2_numpy_dataframe_adapter_and_zero_com_pptx() -> None:
        import zipfile
        import numpy as np  # type: ignore[import-not-found]
        import olefile  # type: ignore[import-not-found]
        import pandas as pd  # type: ignore[import-not-found]
        import pptx  # type: ignore[import-not-found]
        from pptx.util import Cm, Inches  # type: ignore[import-not-found]

        app.new_project()

        # 1. Pandas DataFrame + attrs['units'] / attrs['long_names'] + NaN round-trip
        df_src = pd.DataFrame(
            {
                "Strain": [0.0, 0.5, 1.0, 2.0, 3.0],
                "Stress": [0.0, 350.0, np.nan, 495.0, 535.0],
            }
        )
        df_src.attrs["units"] = {"Strain": "%", "Stress": "MPa"}
        df_src.attrs["long_names"] = {"Strain": "Nominal strain", "Stress": "Nominal stress"}

        t_pd = app.create_table(df_src, name="W13_PandasTbl")
        stress_meta = t_pd.get_meta("Stress")
        assert stress_meta.get("units") == "MPa" and stress_meta.get("long_name") == "Nominal stress", stress_meta
        assert t_pd.get_cell(2, "Stress") is None, "NaN in DataFrame was not stored as invalid cell!"

        df_rt = t_pd.to_dataframe(backend="pandas")
        assert list(df_rt.columns) == ["Strain", "Stress"]
        assert df_rt.attrs.get("units", {}).get("Stress") == "MPa", df_rt.attrs
        assert df_rt.attrs.get("long_names", {}).get("Strain") == "Nominal strain", df_rt.attrs
        np_tbl = t_pd.to_numpy()
        assert np_tbl.shape == (5, 2) and np.isnan(np_tbl[2, 1]) and abs(np_tbl[4, 1] - 535.0) < 1e-9

        # Direct DataFrame & 1D NumPy ndarray `plot_xy` + 2D NumPy Matrix round-trip
        g_df = app.plot_xy(df_src.dropna(), "Strain", "Stress", style="line_symbol", graph_name="W13-DfGraph")
        g_df.slide_style()
        x_np = np.linspace(0.0, 2.0 * math.pi, 25)
        y_np = np.cos(x_np) * 100.0
        g_np = app.plot_xy(x_np, y_np, style="line", graph_name="W13-NpGraph")
        g_np.isij_style()

        mat_arr = np.array([[1.5, 2.5, np.nan], [4.5, 5.5, 6.5]], dtype=np.float64)
        m_np = app.create_matrix(mat_arr, name="W13_NpMat", x_min=0.0, x_max=2.0, y_min=0.0, y_max=1.0)
        mat_rt = m_np.to_numpy()
        assert mat_rt.shape == (2, 3) and np.isnan(mat_rt[0, 2]) and abs(mat_rt[1, 2] - 6.5) < 1e-9

        # Optional Polars verification if installed
        try:
            import polars as pl  # type: ignore[import-not-found]

            pl_df = pl.DataFrame({"Q": [1.0, 2.0, 3.0], "I": [100.0, 250.0, 90.0]})
            t_pl = app.create_table(pl_df, name="W13_PolarsTbl")
            pl_back = t_pl.to_dataframe(backend="polars")
            assert pl_back.shape == (3, 2)
        except ImportError:
            pass

        # 2. Zero-COM PowerPoint Native OLE Deck (`app.to_pptx` & `g.to_pptx`)
        deck_path = tmp_dir / "w13_native_ole_deck.pptx"
        pptx_res = app.to_pptx(
            deck_path,
            [g_df, g_np],
            per_slide=2,
            titles=["Al-Zn-Mg Mechanical & Harmonic Summary"],
            dpi=300,
        )
        assert pptx_res.get("ok") is True and deck_path.is_file() and deck_path.stat().st_size > 4000, pptx_res
        assert pptx_res.get("slide_count") == 1 and pptx_res.get("embedded_count") == 2

        with zipfile.ZipFile(deck_path, "r") as zf:
            z_entries = set(zf.namelist())
            ole_entries = sorted(e for e in z_entries if e.startswith("ppt/embeddings/oleObject"))
            emf_entries = sorted(e for e in z_entries if e.startswith("ppt/media/image") and e.endswith(".emf"))
            assert len(ole_entries) == 2 and len(emf_entries) == 2, z_entries

            s1_xml = zf.read("ppt/slides/slide1.xml").decode("utf-8")
            assert 'progId="ToriGraph.Graph.1"' in s1_xml
            assert 'descr="torigraph:W13-DfGraph"' in s1_xml
            assert 'descr="torigraph:W13-NpGraph"' in s1_xml

            for emf_e in emf_entries:
                emf_raw = zf.read(emf_e)
                assert len(emf_raw) >= 88 and emf_raw[:4] == b"\x01\x00\x00\x00" and emf_raw[40:44] == b" EMF"

            for ole_e in ole_entries:
                ole_raw = zf.read(ole_e)
                with olefile.OleFileIO(ole_raw) as ole:
                    assert ole.root.clsid.upper() == "8E6B9541-3C74-4D28-A915-7D84E5A12001", ole.root.clsid
                    assert ole.exists("\x01CompObj") and ole.exists("\x01Ole"), ole.listdir()
                    assert ole.exists("ToriGraphProject") and ole.exists("SciDAVisProject"), ole.listdir()
                    proj_xml = ole.openstream("ToriGraphProject").read().decode("utf-8", errors="replace")
                    assert "W13_PandasTbl" in proj_xml and "W13-DfGraph" in proj_xml

        # `path == deck` safety check (must refuse overwrite unless overwrite=True)
        blocked_overwrite = False
        try:
            app.to_pptx(deck_path, [g_df], deck=deck_path)
        except ValueError:
            blocked_overwrite = True
        assert blocked_overwrite, "to_pptx failed to block path == deck when overwrite=False!"

        # Idempotent `replace=True, overwrite=True` preserving slide count and zero orphan OLE bloat
        upd_res = app.to_pptx(deck_path, [g_df], deck=deck_path, replace=True, overwrite=True)
        assert upd_res.get("replaced_count") == 1 and upd_res.get("slide_count") == 1
        with zipfile.ZipFile(deck_path, "r") as zf_upd:
            ole_after = [e for e in zf_upd.namelist() if e.startswith("ppt/embeddings/oleObject")]
            assert len(ole_after) == 2, f"Orphan OLE objects leaked after replace=True: {ole_after}"

        # Named shape replacement `replace=[(2, "Picture 3")]` + explicit `box=(left_cm, top_cm, width_cm)`
        tmpl_pptx = tmp_dir / "w13_template.pptx"
        prs_t = pptx.Presentation()
        prs_t.slide_width = Inches(13.333)
        prs_t.slide_height = Inches(7.5)
        prs_t.slides.add_slide(prs_t.slide_layouts[6])
        sl2 = prs_t.slides.add_slide(prs_t.slide_layouts[6])
        ph = sl2.shapes.add_shape(1, Cm(2.5), Cm(2.0), Cm(11.0), Cm(11.0))
        ph.name = "Picture 3"
        prs_t.save(str(tmpl_pptx))

        filled_pptx = tmp_dir / "w13_filled_named.pptx"
        rep_res = app.to_pptx(filled_pptx, [g_np], deck=tmpl_pptx, replace=[(2, "Picture 3")])
        assert rep_res.get("replaced_count") == 1 and rep_res.get("slides_modified") == [2]

        box_pptx = tmp_dir / "w13_box_single.pptx"
        box_res = g_np.to_pptx(box_pptx, box=(2.0, 2.5, 12.0))
        assert abs(box_res["placements"][0]["width_cm"] - 12.0) < 0.05

    runner.check(
        "13.2 NumPy/Pandas/Polars Zero-Friction Adapter & Zero-COM PowerPoint Native OLE Deck (`to_pptx` + `olefile`)",
        _w13_2_numpy_dataframe_adapter_and_zero_com_pptx,
    )

    def _w13_3_sample_gallery_category_stacked100_and_polar() -> None:
        app.new_project()

        # 1. Sample Datasets Gallery
        samples = app.list_sample_datasets()
        assert len(samples) >= 5, f"Expected >= 5 built-in sample datasets, got {samples}"
        first_id = str(samples[0].get("id") or samples[0].get("name"))
        loaded = app.load_sample_dataset(first_id, auto_plot=True)
        assert loaded.get("ok") is True and len(app.list_windows()) >= 2, f"load_sample_dataset failed: {loaded}"

        # 2. Category Bar Chart under `isij_style()` + `set_category_labels`
        app.new_project()
        t_cat = app.create_table(
            {"Condition": ["As-Q", "120C", "160C", "RRA"], "HV": [95.0, 182.0, 168.0, 195.0]},
            units={"HV": "HV0.1"},
            name="W13_CatBarTbl",
        )
        g_cat = app.plot_bars(t_cat, x_col="Condition", y_cols="HV", graph_name="W13-CatBars")
        g_cat.isij_style()
        cat_snap = g_cat.inspect(include_ticks=True)
        x_lbls = [str(s) for s in cat_snap["layers"][0]["axes"]["x"].get("tick_labels", []) if str(s).strip()]
        for expected_cat in ("As-Q", "120C", "160C", "RRA"):
            assert expected_cat in x_lbls, f"Category {expected_cat!r} dropped under isij_style()! Got {x_lbls}"
        assert len(x_lbls) == len(set(x_lbls)), f"Duplicate fractional category ticks detected: {x_lbls}"

        g_cat.set_category_labels("x", ["WQ", "T4", "T6", "T73"])
        upd_lbls = [str(s) for s in g_cat.inspect(include_ticks=True)["layers"][0]["axes"]["x"].get("tick_labels", []) if str(s).strip()]
        assert "T6" in upd_lbls and "T73" in upd_lbls, f"set_category_labels did not update ticks: {upd_lbls}"

        # 3. 100% Normalized Stacked Bars on Text X Column
        t_stk = app.create_table(
            {
                "Specimen": ["Al-T4", "Al-T6", "Al-T73"],
                "Transgranular": [25.0, 60.0, 40.0],
                "Intergranular": [75.0, 40.0, 60.0],
            },
            name="W13_Stack100Tbl",
        )
        g_stk100 = app.plot_stacked_bars(
            t_stk,
            x_col="Specimen",
            y_cols=["Transgranular", "Intergranular"],
            percent_100=True,
            graph_name="W13-Stack100",
        )
        g_stk100.isij_style()
        stk_snap = g_stk100.inspect(include_ticks=True)
        stk_curves = stk_snap["layers"][0]["curves"]
        assert len(stk_curves) >= 2
        assert stk_curves[0]["x_max"] > stk_curves[0]["x_min"], f"Text-X stacked bars collapsed at x=0: {stk_curves}"
        top_ymax = max(float(c.get("y_max", 0.0)) for c in stk_curves)
        assert abs(top_ymax - 100.0) < 1.0, f"percent_100=True stacked bars top y_max={top_ymax} != 100.0"

        # 4. Polar / Radar Plot (`plot.polar`)
        thetas = [float(k * 30) for k in range(13)]
        r_vals = [450.0 + 40.0 * math.cos(2.0 * math.radians(th)) for th in thetas]
        t_pol = app.create_table({"Theta": thetas, "R_MPa": r_vals}, name="W13_PolarTbl")
        g_pol = app.plot_polar(t_pol, theta_col="Theta", r_cols=["R_MPa"], theta_in_radians=False, graph_name="W13-Polar")
        pol_png = tmp_dir / "w13_polar.png"
        g_pol.export(pol_png, dpi=300)
        assert parse_png_metadata_and_pixels(pol_png)["non_white_interior"] >= 25
        assert_graph_arial(app, g_pol.name, "W13 Polar Plot")

    runner.check(
        "13.3 Sample Gallery, Category Bars (`isij_style` + `set_category_labels`), `100%` Stacked Bars & Polar Plot",
        _w13_3_sample_gallery_category_stacked100_and_polar,
    )

    def _w13_4_kissinger_kocks_mecking_xrd_and_vasp() -> None:
        app.new_project()

        # 1. Kissinger TDS Activation Energy (`analyze_kissinger`): exact Ed = 28.5 kJ/mol
        r_gas = 8.314462618  # J/(mol*K)
        ed_true_kj = 28.5
        tp_kelvin = [410.0, 430.0, 450.0, 470.0]
        betas_k_min = [
            (tp * tp) * math.exp(16.0 - (ed_true_kj * 1000.0) / (r_gas * tp))
            for tp in tp_kelvin
        ]
        tp_celsius = [tp - 273.15 for tp in tp_kelvin]
        t_kis = app.create_table(
            {"HeatingRate": betas_k_min, "Tp_C": tp_celsius},
            name="W13_KissingerTbl",
        )
        kis_res = app.analyze_kissinger(
            t_kis,
            rate_col="HeatingRate",
            temp_col="Tp_C",
            rate_in_k_per_hour=False,
            temp_in_celsius=True,
            top_axis_in_celsius=True,
        )
        assert kis_res.get("ok") is True, f"analyze_kissinger failed: {kis_res}"
        ea_kj = (
            kis_res.get("ea_kj_mol")
            or kis_res.get("activation_energy_kj_mol")
            or kis_res.get("ed_kj_mol")
            or kis_res.get("ea_kjmol")
        )
        assert ea_kj is not None and abs(float(ea_kj) - ed_true_kj) < 0.6, (
            f"Kissinger Ea={ea_kj} deviated from {ed_true_kj} kJ/mol: {kis_res}"
        )

        # 2. Kocks-Mecking Work-Hardening Rate & Considere Necking (`kocks_mecking`)
        e_mod = 70000.0
        eps_true_grid = [0.0005 * i for i in range(321)]  # 0 .. 0.16 true strain
        eng_e: List[float] = []
        eng_s: List[float] = []
        for et in eps_true_grid:
            if et <= 250.0 / e_mod:
                st = e_mod * et
            else:
                ep = et - 250.0 / e_mod
                st = 250.0 + 320.0 * (1.0 - math.exp(-16.0 * ep))
            ee = math.exp(et) - 1.0
            eng_e.append(ee)
            eng_s.append(st / (1.0 + ee))
        t_km = app.create_table({"Strain": eng_e, "Stress": eng_s}, units={"Strain": "-", "Stress": "MPa"}, name="W13_VoceTbl")
        g_km = app.plot_xy(t_km, "Strain", "Stress", style="line", graph_name="W13-VoceGraph")
        km_res = g_km.analyze_work_hardening(smooth_window=11, create_plot=True)
        assert km_res.get("ok") is True, f"kocks_mecking failed: {km_res}"

        # 3. XRD Bragg, Scherrer & Williamson-Hall (`analyze_xrd` & `bragg_convert`)
        lam = 1.540598  # Cu Ka1 (Angstrom)
        a0 = 4.0495     # fcc-Al lattice parameter (Angstrom)
        d_cryst_nm = 42.0
        eps_wh = 0.0018
        hkl_sq = [3, 4, 8, 11]  # (111), (200), (220), (311)
        amps_xrd = [1000.0, 480.0, 290.0, 310.0]
        peaks_spec: List[Tuple[float, float, float]] = []
        for h2, amp in zip(hkl_sq, amps_xrd):
            d_hkl = a0 / math.sqrt(float(h2))
            sin_th = lam / (2.0 * d_hkl)
            th_rad = math.asin(sin_th)
            two_th = 2.0 * math.degrees(th_rad)
            beta_phys_rad = (0.90 * lam) / (d_cryst_nm * 10.0 * math.cos(th_rad)) + 4.0 * eps_wh * math.tan(th_rad)
            fwhm_deg = math.sqrt((math.degrees(beta_phys_rad)) ** 2 + 0.05**2)
            peaks_spec.append((two_th, amp, fwhm_deg))

        tt_grid = [30.0 + 0.05 * i for i in range(1101)]  # 30 .. 85 deg
        inten_grid = [
            20.0 + sum(pseudo_voigt(tt, amp, c_tt, fw, mu=0.35) for c_tt, amp, fw in peaks_spec)
            for tt in tt_grid
        ]
        t_xrd = app.create_table(
            {"TwoTheta": tt_grid, "Intensity": inten_grid},
            units={"TwoTheta": "deg", "Intensity": "counts"},
            name="W13_XrdTbl",
        )
        g_xrd = app.plot_xy(t_xrd, "TwoTheta", "Intensity", style="line", graph_name="W13-XrdGraph")
        xrd_res = g_xrd.analyze_xrd(
            wavelength=lam,
            instr_fwhm=0.05,
            scherrer_k=0.90,
            crystal_system="fcc",
            add_top_d_axis=True,
            create_wh_plot=True,
        )
        assert xrd_res.get("ok") is True and len(xrd_res.get("peaks", [])) >= 3, f"analyze_xrd failed: {xrd_res}"

        g_bragg = app.plot_xy(t_xrd, "TwoTheta", "Intensity", style="line", graph_name="W13-BraggGraph")
        bragg_res = g_bragg.bragg_convert(wavelength=lam, mode="d_spacing")
        assert bragg_res.get("ok") is True, f"bragg_convert failed: {bragg_res}"

        # 4. VASP / DFT Spin-Polarized DOS & Electronic Band Structure
        e_grid = [-4.0 + 0.1 * i for i in range(121)]  # -4 .. +8 eV (raw, E_F = 2.0 eV)
        dos_up = [math.exp(-((e - 0.5) / 1.1) ** 2) * 3.5 + math.exp(-((e - 4.2) / 0.8) ** 2) * 2.2 for e in e_grid]
        dos_dn = [math.exp(-((e - 0.9) / 1.1) ** 2) * 3.1 + math.exp(-((e - 4.5) / 0.8) ** 2) * 2.0 for e in e_grid]
        t_dos = app.create_table(
            {"Energy": e_grid, "DOS_Up": dos_up, "DOS_Down": dos_dn},
            name="W13_VaspDosTbl",
        )
        dos_res = app.plot_vasp_dos(
            t_dos,
            energy_col="Energy",
            dos_cols=["DOS_Up"],
            down_dos_cols=["DOS_Down"],
            fermi_energy=2.0,
            vertical_energy=False,
        )
        assert dos_res.get("ok") is True and dos_res.get("graph"), f"plot_vasp_dos failed: {dos_res}"
        assert_graph_arial(app, str(dos_res["graph"]), "W13 VASP DOS")

        k_pts = [0.05 * i for i in range(61)]  # 0.0 .. 3.0
        vb1 = [-2.2 - 0.8 * math.cos(math.pi * k / 1.0) for k in k_pts]
        vb2 = [-1.1 + 0.5 * math.cos(math.pi * k / 1.0) for k in k_pts]  # VBM = -0.6 eV
        cb1 = [1.4 - 0.6 * math.cos(math.pi * k / 1.0) for k in k_pts]   # CBM = +0.8 eV
        cb2 = [2.8 + 0.5 * math.sin(math.pi * k / 1.0) for k in k_pts]
        t_bnd = app.create_table(
            {"k_dist": k_pts, "B1": vb1, "B2": vb2, "B3": cb1, "B4": cb2},
            name="W13_VaspBandsTbl",
        )
        bnd_res = app.plot_vasp_bands(
            t_bnd,
            k_col="k_dist",
            band_cols=["B1", "B2", "B3", "B4"],
            k_ticks=[0.0, 1.0, 2.0, 3.0],
            k_labels=["Γ", "X", "M", "Γ"],
            fermi_energy=0.0,
            e_min=-4.0,
            e_max=4.0,
        )
        assert bnd_res.get("ok") is True and bnd_res.get("graph"), f"plot_vasp_bands failed: {bnd_res}"
        assert_graph_arial(app, str(bnd_res["graph"]), "W13 VASP Bands")

    runner.check(
        "13.4 Kissinger TDS `Ea`, Kocks-Mecking `dσ/dε`, XRD Williamson-Hall & VASP DOS/Bands",
        _w13_4_kissinger_kocks_mecking_xrd_and_vasp,
    )

    def _w13_5_round2_sdk_autonomy_plot_and_matsci_regressions() -> None:
        app.new_project()

        # 1. Grouped multi-series `plot_bars` (default `bar_gap=None` + `y_err_cols`) & Horizontal Category Bars
        t_grp = app.create_table(
            {
                "Temper": ["T4", "T6", "T73"],
                "YS": [260.0, 485.0, 430.0],
                "UTS": [410.0, 560.0, 495.0],
                "YS_Err": [8.0, 10.0, 9.0],
                "UTS_Err": [12.0, 11.0, 10.0],
            },
            units={"YS": "MPa", "UTS": "MPa"},
            name="W13R2_GrpBarsTbl",
        )
        g_grp = app.plot_bars(
            t_grp,
            x_col="Temper",
            y_cols=["YS", "UTS"],
            y_err_cols=["YS_Err", "UTS_Err"],
            graph_name="W13R2-GrpBars",
        )
        g_grp.slide_style()
        grp_curves = g_grp.info()["layers"][0]["curves"]
        assert len(grp_curves) >= 4, f"Expected 2 bar + 2 error curves from y_err_cols, got {grp_curves}"

        g_hbar = app.plot_bars(
            t_grp,
            x_col="Temper",
            y_cols=["YS", "UTS"],
            horizontal=True,
            graph_name="W13R2-HorizBars",
        )
        g_hbar.isij_style()
        hbar_axes = g_hbar.inspect(include_ticks=True)["layers"][0]["axes"]
        assert float(hbar_axes["x"]["max"]) > 100.0, (
            f"HorizontalBars on Text X locked xBottom to category range instead of numeric values: {hbar_axes['x']}"
        )
        y_hbar_lbls = [str(s) for s in hbar_axes["y"].get("tick_labels", []) if str(s).strip()]
        assert "T6" in y_hbar_lbls, f"HorizontalBars missing category labels on yLeft: {y_hbar_lbls}"

        # 2. Stacked Histogram (`stacked=True`) & Pareto Category Ticks Under `isij_style()`
        t_shist = app.create_table(
            {
                "H1": [10.0 + 1.5 * (i % 7) for i in range(28)],
                "H2": [14.0 + 1.8 * (i % 6) for i in range(28)],
            },
            name="W13R2_StackHistTbl",
        )
        g_shist = app.plot_histogram(t_shist, cols=["H1", "H2"], stacked=True, graph_name="W13R2-StackedHist")
        sh_info = g_shist.info()
        assert sh_info["layer_count"] == 2 and all(ly["curve_count"] >= 1 for ly in sh_info["layers"]), sh_info

        t_par = app.create_table(
            {
                "Defect": ["Porosity", "Inclusion", "Hot crack", "Lack of fusion", "Undercut"],
                "Count": [48.0, 31.0, 15.0, 9.0, 4.0],
            },
            name="W13R2_ParetoTbl",
        )
        g_par = app.plot_pareto(t_par, value_col="Count", category_col="Defect", graph_name="W13R2-Pareto")
        g_par.isij_style()
        par_x_lbls = [
            str(s)
            for s in g_par.inspect(include_ticks=True)["layers"][0]["axes"]["x"].get("tick_labels", [])
            if str(s).strip()
        ]
        for defect_name in ("Porosity", "Inclusion", "Hot crack"):
            assert defect_name in par_x_lbls, f"Pareto dropped '{defect_name}' under isij_style(): {par_x_lbls}"

        # 3. Polar Crash-Safety (`plot_polar` -> `create_table` / rename / `close_window`), `plot_function(type='polar')`,
        #    Specialty `inspect`/`auto_layout` & `to_python_script` Fidelity (Polar, Shaded Band, Category Bars)
        thetas_r2 = [float(k * 30) for k in range(13)]
        t_pol_r2 = app.create_table(
            {"Theta": thetas_r2, "R": [70.0 + 8.0 * math.cos(math.radians(2.0 * th)) for th in thetas_r2]},
            name="W13R2_PolarTbl",
        )
        g_pol_r2 = app.plot_polar(t_pol_r2, theta_col="Theta", r_cols=["R"], graph_name="W13R2-PolarSafe")
        t_after_pol = app.create_table({"X": [1.0, 2.0, 3.0], "Y": [2.0, 4.0, 6.0]}, name="W13R2_AfterPolarTbl")
        t_after_pol.set_meta("Y", name="Y_Renamed")
        assert app.ping().get("pong") is True

        pol_insp = g_pol_r2.inspect()
        assert "DISABLED_AXIS_HAS_TITLE_OR_CURVE" not in set(pol_insp.get("warning_codes", [])), pol_insp.get("warnings")
        g_pol_r2.auto_layout()
        pol_src = g_pol_r2.to_python_script(standalone=False)
        assert "plot_polar" in pol_src, f"to_python_script on polar plot omitted plot_polar:\n{pol_src}"

        g_pfunc = app.plot_function(
            "70 + 8*cos(4*x)",
            x_min=0.0,
            x_max=2.0 * math.pi,
            type="polar",
            graph_name="W13R2-PolarFunc",
        )
        assert g_pfunc.info()["layers"][0]["curve_count"] >= 1
        app.close_window(g_pol_r2)
        assert app.ping().get("pong") is True

        g_band = app.plot_xy(t_after_pol, "X", "Y_Renamed", graph_name="W13R2-ShadedBand")
        g_band.add_shaded_band([1.0, 2.0, 3.0], [1.5, 3.5, 5.5], [2.5, 4.5, 6.5], label="CI95")
        band_src = g_band.to_python_script(standalone=False)
        assert "add_shaded_band" in band_src, f"to_python_script dropped shaded band:\n{band_src}"
        compile(g_grp.to_python_script(standalone=False), "<cat_bars_to_py>", "exec")

        # 4. Atomic `app.batch` Multi-Step Graph Undo/Redo & `[P2-R2-1]` SDK Keyword Parity
        g_bu = app.plot_xy(t_after_pol, "X", "Y_Renamed", graph_name="W13R2-BatchUndo")
        g_bu.set_title("BeforeBatchTitle")
        with app.batch(atomic=True):
            g_bu.set_title("MidBatchTitle")
            g_bu.set_title("AfterBatchTitle")
        assert g_bu.info()["layers"][0].get("title") == "AfterBatchTitle"
        g_bu.undo()
        assert g_bu.info()["layers"][0].get("title") == "BeforeBatchTitle", g_bu.info()["layers"][0]
        g_bu.redo()
        assert g_bu.info()["layers"][0].get("title") == "AfterBatchTitle", g_bu.info()["layers"][0]

        assert len(app.list_sample_datasets(category="")) >= 5
        assert app.set_language(lang="ja").get("ok") is not False
        assert len(t_grp.get_column("YS", compact=True, include_masked=False)) == 3
        assert t_grp.row_stats(start_row=0, end_row=1).get("ok") is not False
        m_kw = app.create_matrix(rows=2, cols=2, name="W13R2_MatKw")
        m_kw.set_data(rows_2d=[[1.0, 2.0], [3.0, 4.0]])
        g_hm = app.plot_heatmap(m_kw, palette="Cividis", graph_name="W13R2-Heatmap")
        assert g_hm.name
        g_bu.add_curve(t_after_pol, "Y_Renamed", x_col="X")
        assert g_bu.move_curve(curve=0, direction="down").get("ok") is True

        # 5. Round-2 Domain Solvers (`weibull_fit(mle=True)`, `nonparametric_test(add_brackets=True)`,
        #    `kocks_mecking(plot_mode='kocks_mecking')`, `analyze_xrd(crystal_system='hcp', c_over_a=1.587)`)
        t_wb = app.create_table(
            {
                "MPa": [410.0, 435.0, 452.0, 468.0, 480.0, 495.0, 512.0, 530.0, 550.0, 550.0],
                "Cens": [1, 1, 1, 1, 1, 1, 1, 1, 0, 0],
            },
            name="W13R2_WeibullTbl",
        )
        wb_mle = app.weibull(t_wb, "MPa", censor_col="Cens", mle=True, plot=True)
        assert wb_mle.get("ok") is True and wb_mle.get("censored_count") == 2 and wb_mle.get("used_mle") is True, wb_mle

        t_np3 = app.create_table(
            {
                "G1": [10.0, 11.0, 12.0, 10.5, 11.5],
                "G2": [22.0, 24.0, 23.0, 25.0, 21.5],
                "G3": [35.0, 38.0, 36.0, 37.0, 39.0],
            },
            name="W13R2_NpBoxTbl",
        )
        g_np_box = app.plot_box(t_np3, cols=["G1", "G2", "G3"], graph_name="W13R2-NpBox")
        np_res = app.nonparametric(
            t_np3,
            ["G1", "G2", "G3"],
            test="kruskal_wallis",
            graph=g_np_box,
            add_brackets=True,
        )
        assert np_res.get("ok") is True, np_res

        e_mod2 = 70000.0
        ee_v: List[float] = []
        es_v: List[float] = []
        for i in range(161):
            et = 0.001 * i
            st = e_mod2 * et if et <= 250.0 / e_mod2 else 250.0 + 320.0 * (1.0 - math.exp(-16.0 * (et - 250.0 / e_mod2)))
            ee = math.exp(et) - 1.0
            ee_v.append(ee)
            es_v.append(st / (1.0 + ee))
        t_voce2 = app.create_table({"Strain": ee_v, "Stress": es_v}, units={"Strain": "-", "Stress": "MPa"}, name="W13R2_VoceTbl")
        g_voce2 = app.plot_xy(t_voce2, "Strain", "Stress", style="line", graph_name="W13R2-Voce")
        km_classic = g_voce2.analyze_work_hardening(smooth_window=9, create_plot=True, plot_mode="kocks_mecking")
        assert km_classic.get("ok") is True and km_classic.get("graph"), km_classic

        lam_cu = 1.540598
        a_ti = 2.950
        ca_ti = 1.587
        c_ti = a_ti * ca_ti
        hcp_hkls = [(1, 0, 0, 520.0), (0, 0, 2, 580.0), (1, 0, 1, 1000.0), (1, 0, 2, 340.0)]
        hcp_peaks: List[Tuple[float, float, float]] = []
        for h_i, k_i, l_i, amp_i in hcp_hkls:
            inv_d2 = (4.0 / 3.0) * float(h_i * h_i + h_i * k_i + k_i * k_i) / (a_ti * a_ti) + float(l_i * l_i) / (c_ti * c_ti)
            d_val = 1.0 / math.sqrt(inv_d2)
            two_th_i = 2.0 * math.degrees(math.asin(lam_cu / (2.0 * d_val)))
            hcp_peaks.append((two_th_i, amp_i, 0.22))
        tt_hcp = [30.0 + 0.05 * i for i in range(601)]
        inten_hcp = [15.0 + sum(pseudo_voigt(tt, a_p, c_p, w_p, mu=0.35) for c_p, a_p, w_p in hcp_peaks) for tt in tt_hcp]
        t_hcp = app.create_table({"TwoTheta": tt_hcp, "Intensity": inten_hcp}, name="W13R2_HcpXrdTbl")
        g_hcp = app.plot_xy(t_hcp, "TwoTheta", "Intensity", style="line", graph_name="W13R2-HcpXrd")
        hcp_res = g_hcp.analyze_xrd(
            wavelength=lam_cu,
            instr_fwhm=0.05,
            crystal_system="hcp",
            c_over_a=ca_ti,
            create_wh_plot=False,
        )
        assert hcp_res.get("ok") is True and len(hcp_res.get("peaks", [])) >= 3, hcp_res
        assert any(str(pk.get("hkl", "")).strip() for pk in hcp_res["peaks"]), f"HCP XRD peaks missing hkl labels: {hcp_res}"

        # 6. Round-3 Regression (#304 [P1-R3-01]): in-memory worksheet names "DOSCAR1" / "EIGENVAL1"
        #    must resolve as `table=...` rather than missing disk paths `doscar_path` / `eigenval_path`!
        t_dos_named = app.create_table(
            {
                "E": [-2.0 + 0.5 * i for i in range(9)],
                "TotalDOS": [0.8, 1.4, 2.2, 1.9, 0.5, 1.1, 2.0, 1.5, 0.7],
            },
            name="DOSCAR1",
        )
        dos_by_str = app.plot_vasp_dos(t_dos_named.name, energy_col="E", dos_cols=["TotalDOS"], fermi_energy=0.0)
        assert dos_by_str.get("ok") is True and dos_by_str.get("graph"), f"plot_vasp_dos('DOSCAR1') failed: {dos_by_str}"

        t_eig_named = app.create_table(
            {
                "k": [0.0, 0.5, 1.0, 1.5, 2.0],
                "VB": [-1.4, -0.9, -0.5, -1.0, -1.3],
                "CB": [1.2, 0.9, 0.7, 1.1, 1.5],
            },
            name="EIGENVAL1",
        )
        eig_by_str = app.plot_vasp_bands(
            t_eig_named.name,
            k_col="k",
            band_cols=["VB", "CB"],
            k_ticks=[0.0, 1.0, 2.0],
            k_labels=["Γ", "X", "M"],
            fermi_energy=0.0,
        )
        assert eig_by_str.get("ok") is True and eig_by_str.get("graph"), f"plot_vasp_bands('EIGENVAL1') failed: {eig_by_str}"

        # 7. Round-5 Regression (#329 [P0-R5-1], #330 [P0-R5-01]): live `to_python_script()` + `exec()`
        #    round-trip on Pie, Box, Violin, Single Histogram & Stacked Histogram!
        def _assert_specialty_to_python_exec(src_graph: Any, expected_call: str, min_layers: int = 1) -> None:
            py_code = src_graph.to_python_script(standalone=False, embed_data="inline")
            assert expected_call in py_code, f"Expected '{expected_call}' in generated script:\n{py_code}"
            compile(py_code, f"<{expected_call}_to_py>", "exec")
            app.new_project()
            exec_ns: Dict[str, Any] = {"__builtins__": __builtins__, "app": app, "tg": tg}
            exec(py_code, exec_ns)
            mls_after = [w["name"] for w in app.list_windows() if w.get("type") == "MultiLayer"]
            assert len(mls_after) >= 1, f"Executed {expected_call} script created no MultiLayer:\n{py_code}"
            g_re = app.graph(mls_after[0])
            info_re = g_re.info()
            assert info_re.get("layer_count", 0) >= min_layers and info_re["layers"][0]["curve_count"] >= 1, info_re

        app.new_project()
        t_pie_py = app.create_table(
            {"Phase": ["Alpha", "Eta", "Al3Zr", "S"], "Frac": [58.0, 24.0, 11.0, 7.0]},
            name="W13R5_PieTbl",
        )
        g_pie_py = app.plot_pie(t_pie_py, col="Frac", label_col="Phase", graph_name="W13R5-Pie")
        _assert_specialty_to_python_exec(g_pie_py, "plot_pie")

        app.new_project()
        t_bv_py = app.create_table(
            {
                "T6": [480.0, 495.0, 502.0, 510.0, 518.0, 525.0, 534.0],
                "RRA": [520.0, 535.0, 542.0, 550.0, 558.0, 566.0, 575.0],
            },
            name="W13R5_BoxViolTbl",
        )
        g_box_py = app.plot_box(
            t_bv_py, cols=["T6", "RRA"], notched=True, show_points=True, whisker_mode="minmax", graph_name="W13R5-Box"
        )
        _assert_specialty_to_python_exec(g_box_py, "plot_box")

        app.new_project()
        t_viol_py = app.create_table(
            {
                "T6": [480.0, 495.0, 502.0, 510.0, 518.0, 525.0, 534.0],
                "RRA": [520.0, 535.0, 542.0, 550.0, 558.0, 566.0, 575.0],
            },
            name="W13R5_ViolTbl",
        )
        g_viol_py = app.plot_violin(t_viol_py, cols=["T6", "RRA"], show_points=True, graph_name="W13R5-Violin")
        _assert_specialty_to_python_exec(g_viol_py, "plot_violin")

        app.new_project()
        t_hist_py = app.create_table(
            {
                "GrainT6": [12.0 + 1.5 * (i % 9) + 0.3 * i for i in range(30)],
                "GrainRRA": [18.0 + 1.8 * (i % 8) + 0.25 * i for i in range(30)],
            },
            name="W13R5_HistTbl",
        )
        g_hist_py = app.plot_histogram(
            t_hist_py, cols=["GrainT6", "GrainRRA"], stacked=True, overlay_pdf=True, graph_name="W13R5-StackedHist"
        )
        _assert_specialty_to_python_exec(g_hist_py, "plot_histogram", min_layers=2)

    runner.check(
        "13.5 Round-2 Regressions: Grouped/Horiz Bars, Stacked Hist, Pareto, Polar Safety, Batch Undo & HCP/KM/Weibull MLE",
        _w13_5_round2_sdk_autonomy_plot_and_matsci_regressions,
    )


def main() -> int:
    runner = E2ERunner()
    with tempfile.TemporaryDirectory(prefix="torigraph_e2e_") as tmp:
        tmp_dir = Path(tmp)
        print("[INFO] Launching live ToriGraph headless RPC server (language='ja')...")
        with tg.launch(headless=True, language="ja") as app:
            run_section_0_sdk_proxy_contract(runner, app, tmp_dir)
            run_workflow_1_tds_deconvolution(runner, app)
            refs2 = run_workflow_2_tensile_presets_export(runner, app, tmp_dir)
            run_workflow_3_nonlinear_and_regression(runner, app)
            run_workflow_4_materials_science(runner, app)
            refs5 = run_workflow_5_signal_and_matrix_3d(runner, app)
            combined_refs = {**refs2, **refs5}
            run_workflow_6_stats_gadgets_curvemath_digitizer_project_updater(
                runner, app, tmp_dir, combined_refs
            )
            run_workflow_8_round2_critic_regressions(runner, app, tmp_dir)
            run_workflow_9_round3_critic_regressions(runner, app, tmp_dir)
            run_workflow_10_round4_unified_regressions(runner, app, tmp_dir)
            run_workflow_11_round5_zero_defect_gate(runner, app, tmp_dir)
            run_workflow_12_phase_b_api_sdk_gate(runner, app, tmp_dir)
            run_workflow_13_megawave1_sdk_pptx_matsci_gate(runner, app, tmp_dir)
        run_workflow_7_cli_run_python(runner, tmp_dir)
        run_workflow_8b_named_pipe_cli(runner, tmp_dir)

    print("\n" + "=" * 76)
    print(f"E2E SUMMARY: {len(runner.passed)} passed, {len(runner.failed)} failed")
    print("=" * 76)
    if runner.failed:
        for name, detail in runner.failed:
            print(f"\n--- FAILURE: {name} ---\n{detail}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
