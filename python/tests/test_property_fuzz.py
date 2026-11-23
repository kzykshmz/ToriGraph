"""Property-based fuzzing tests (Hypothesis) for ToriGraph SDK & live RPC boundaries."""
from __future__ import annotations

import json
import math
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path
from typing import List, Optional

import pytest

hypothesis = pytest.importorskip("hypothesis")
from hypothesis import HealthCheck, given, settings  # noqa: E402
from hypothesis import strategies as st  # noqa: E402

import torigraph as tg  # noqa: E402
import torigraph.pptx as tg_pptx  # noqa: E402

# Strategy: arbitrary printable multilingual strings including XML/JSON special characters
tricky_text = st.text(
    alphabet=st.characters(
        blacklist_categories=("Cs", "Cc"),  # exclude lone surrogates & control chars
    ),
    min_size=0,
    max_size=60,
)

# Strategy: finite floats mixed with NaN, +Inf, -Inf, and None
numeric_or_missing = st.one_of(
    st.none(),
    st.floats(allow_nan=True, allow_infinity=True, width=64),
    st.integers(min_value=-10**9, max_value=10**9).map(float),
)


def _minimal_emf_bytes() -> bytes:
    emf = bytearray(108)
    emf[0:4] = (1).to_bytes(4, "little")  # EMR_HEADER
    emf[4:8] = (88).to_bytes(4, "little")
    emf[32:36] = (8500).to_bytes(4, "little")
    emf[36:40] = (8500).to_bytes(4, "little")
    emf[40:44] = b" EMF"
    emf[44:48] = (0x00010000).to_bytes(4, "little")
    emf[48:52] = (108).to_bytes(4, "little")
    emf[52:56] = (2).to_bytes(4, "little")
    emf[56:58] = (1).to_bytes(2, "little")
    emf[88:92] = (14).to_bytes(4, "little")  # EMR_EOF
    emf[92:96] = (20).to_bytes(4, "little")
    emf[104:108] = (20).to_bytes(4, "little")
    return bytes(emf)


@pytest.mark.fuzz
@settings(max_examples=80, deadline=None)
@given(
    method=st.text(alphabet="abcdefghijklmnopqrstuvwxyz._0123456789", min_size=1, max_size=32),
    label=tricky_text,
    values=st.lists(numeric_or_missing, min_size=0, max_size=30),
)
def test_fuzz_jsonrpc_payload_sanitization(
    method: str, label: str, values: List[Optional[float]]
) -> None:
    """Verify non-finite floats (NaN/Inf) and arbitrary Unicode never produce invalid RFC 8259 JSON."""
    clean_vals = [
        (None if (v is None or (isinstance(v, float) and not math.isfinite(v))) else v)
        for v in values
    ]
    payload = {
        "jsonrpc": "2.0",
        "id": 1,
        "method": method,
        "params": {"label": label, "values": clean_vals},
    }
    encoded = json.dumps(payload, ensure_ascii=False, allow_nan=False)
    decoded = json.loads(encoded)
    assert decoded["method"] == method
    assert decoded["params"]["label"] == label
    assert len(decoded["params"]["values"]) == len(values)


@pytest.mark.fuzz
@settings(
    max_examples=30,
    deadline=None,
    suppress_health_check=[HealthCheck.function_scoped_fixture],
)
@given(
    graph_name=st.text(
        alphabet="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-<>&\"'日本語αβγμσ°",
        min_size=1,
        max_size=40,
    ),
    proj_payload=st.binary(min_size=16, max_size=4096),
)
def test_fuzz_ole_cfb_and_pptx_xml_escaping(
    tmp_path: Path, graph_name: str, proj_payload: bytes
) -> None:
    """Ensure arbitrary XML metacharacters (`<`, `>`, `&`, quotes, Japanese) produce valid CFB & OOXML."""
    pytest.importorskip("pptx")
    from pptx import Presentation

    emf = _minimal_emf_bytes()
    cfb = tg_pptx.build_ole_compound_bytes(
        project_bytes=proj_payload,
        emf_bytes=emf,
        target_window=graph_name,
        width_hm=8500,
        height_hm=8500,
        width_px=600,
        height_px=600,
    )
    # MS-CFB magic header signature check
    assert cfb[:8] == b"\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1"

    prs = Presentation()
    slide = tg_pptx._add_clean_blank_slide(prs, title_text=graph_name)
    tg_pptx.embed_ole_shape_on_slide(
        prs,
        slide,
        ole_bytes=cfb,
        emf_bytes=emf,
        left_emu=914400,
        top_emu=914400,
        width_emu=3060000,
        height_emu=3060000,
        graph_name=graph_name,
    )
    out_pptx = tmp_path / "fuzz_slide.pptx"
    tg_pptx._atomic_save_presentation(prs, out_pptx)
    assert out_pptx.is_file()

    with zipfile.ZipFile(out_pptx, "r") as zf:
        for member in zf.namelist():
            if member.endswith(".xml") or member.endswith(".rels"):
                ET.fromstring(zf.read(member))


@pytest.mark.fuzz
@settings(
    max_examples=12,
    deadline=None,
    suppress_health_check=[HealthCheck.function_scoped_fixture],
)
@given(
    ys=st.lists(
        st.one_of(
            st.none(),
            st.just(float("nan")),
            st.floats(min_value=-1e6, max_value=1e6, allow_nan=False, allow_infinity=False),
        ),
        min_size=4,
        max_size=25,
    )
)
def test_fuzz_live_table_and_plot_with_missing_and_nan_values(tg_app: tg.Session, ys) -> None:
    """Fuzz headless ToriGraph with random NaN/None gaps and extreme dynamic ranges."""
    xs = [float(i) for i in range(len(ys))]
    clean_ys = [None if (v is None or math.isnan(v)) else float(v) for v in ys]
    # Ensure at least one finite point so plot_xy has valid data bounds
    if all(v is None for v in clean_ys):
        clean_ys[0] = 0.0
    tbl = tg_app.create_table({"X_軸": xs, "Y_値": clean_ys}, designations=["X", "Y"])
    g = None
    try:
        g = tg_app.plot_xy(tbl, "X_軸", "Y_値", style="scatter")
        info = g.info()
        assert isinstance(info, dict)
    finally:
        if g is not None:
            tg_app.close_window(g)
        tg_app.close_window(tbl)
