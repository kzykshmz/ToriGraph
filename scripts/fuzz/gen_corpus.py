#!/usr/bin/env python3
"""Generate an adversarial test-data corpus for ToriGraph import / analysis fuzzing.

Standard library only.  Every file is deterministic (fixed seed) so a crash found by
``rpc_fuzz.py`` can be reproduced from the file name alone.

Usage::

    python scripts/fuzz/gen_corpus.py [OUT_DIR]      # default: build-fuzz/corpus

Layout of OUT_DIR::

    csv/       delimited text: encodings, line endings, ragged rows, NaN/Inf, dates ...
    xlsx/      hand-built OOXML workbooks, valid and malformed
    project/   .sciprj project XML (valid seeds + mutated / truncated variants)
    opj/       truncated / bit-flipped copies of the bundled Origin .opj samples
    series/    JSON numeric series for analysis RPCs (see ``SERIES`` below)
    manifest.json  name -> {kind, expect} summary
"""

from __future__ import annotations

import io
import json
import math
import random
import struct
import sys
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
RNG = random.Random(20261128)

MANIFEST: dict[str, dict] = {}


def _put(out: Path, rel: str, data: bytes | str, kind: str, expect: str = "no-crash") -> None:
    p = out / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    if isinstance(data, str):
        data = data.encode("utf-8")
    p.write_bytes(data)
    MANIFEST[rel] = {"kind": kind, "expect": expect, "bytes": len(data)}


# ---------------------------------------------------------------------------
# Numeric series (shared by CSV generation and the analysis fuzzer)
# ---------------------------------------------------------------------------

def _series() -> dict[str, list]:
    n = 200
    xs = [i * 0.05 for i in range(n)]
    s: dict[str, list] = {}
    s["empty"] = []
    s["single"] = [1.0]
    s["two"] = [1.0, 2.0]
    s["three"] = [1.0, 4.0, 9.0]
    s["constant"] = [3.0] * 50
    s["zeros"] = [0.0] * 50
    s["linear"] = [2.0 * x + 1.0 for x in xs]
    s["sine"] = [math.sin(2 * math.pi * x) for x in xs]
    s["gauss_peak"] = [math.exp(-((x - 5) ** 2) / 0.1) + 0.01 * RNG.random() for x in xs]
    s["two_peaks"] = [math.exp(-((x - 3) ** 2) / 0.05) + 0.6 * math.exp(-((x - 6) ** 2) / 0.2) for x in xs]
    s["exp_decay"] = [5.0 * math.exp(-x / 2.0) for x in xs]
    s["noisy"] = [RNG.gauss(0, 1) for _ in xs]
    s["huge"] = [1e300 * (1 + i % 3) for i in range(50)]
    s["tiny"] = [1e-300 * (1 + i % 3) for i in range(50)]
    s["subnormal"] = [5e-324 * (i + 1) for i in range(50)]
    s["mixed_sign_huge"] = [((-1) ** i) * 1e308 for i in range(50)]
    s["negative"] = [-abs(math.sin(x)) - 0.1 for x in xs]
    s["with_nan"] = [float("nan") if i % 7 == 3 else math.cos(x) for i, x in enumerate(xs)]
    s["with_inf"] = [float("inf") if i == 10 else (float("-inf") if i == 20 else x) for i, x in enumerate(xs)]
    s["all_nan"] = [float("nan")] * 30
    s["descending"] = list(reversed(xs))
    s["dup_x"] = [float(i // 3) for i in range(60)]
    s["unsorted"] = RNG.sample(xs, len(xs))
    s["step"] = [0.0] * 50 + [1.0] * 50
    s["spike"] = [0.0] * 99 + [1e6]
    s["large_n"] = [math.sin(i * 0.001) for i in range(100_000)]
    s["int_like"] = [float(i) for i in range(-50, 50)]
    s["alternating"] = [float((-1) ** i) for i in range(64)]
    return s


SERIES = _series()


def _json_safe(v: float):
    if isinstance(v, float) and (math.isnan(v) or math.isinf(v)):
        return repr(v)  # "nan", "inf", "-inf"
    return v


# ---------------------------------------------------------------------------
# CSV / delimited text
# ---------------------------------------------------------------------------

def gen_csv(out: Path) -> None:
    good = "x,y,z\n" + "".join(f"{i},{i*i},{math.sin(i):.6f}\n" for i in range(20))
    _put(out, "csv/basic.csv", good, "csv", "import-ok")
    _put(out, "csv/empty.csv", b"", "csv")
    _put(out, "csv/only_newlines.csv", "\n\n\n\r\n", "csv")
    _put(out, "csv/header_only.csv", "a,b,c\n", "csv")
    _put(out, "csv/no_trailing_newline.csv", "a,b\n1,2\n3,4", "csv", "import-ok")
    _put(out, "csv/bom_utf8.csv", b"\xef\xbb\xbf" + good.encode(), "csv", "import-ok")
    _put(out, "csv/utf16le_bom.csv", b"\xff\xfe" + good.encode("utf-16-le"), "csv")
    _put(out, "csv/utf16be_bom.csv", b"\xfe\xff" + good.encode("utf-16-be"), "csv")
    _put(out, "csv/crlf.csv", good.replace("\n", "\r\n"), "csv", "import-ok")
    _put(out, "csv/cr_only.csv", good.replace("\n", "\r"), "csv")
    _put(out, "csv/mixed_eol.csv", "a,b\r\n1,2\n3,4\r5,6\r\n", "csv")
    _put(out, "csv/ragged.csv", "a,b,c\n1\n1,2\n1,2,3\n1,2,3,4,5\n,,\n", "csv")
    _put(out, "csv/tabs.tsv", good.replace(",", "\t"), "csv", "import-ok")
    _put(out, "csv/semicolon_euro.csv", "x;y\n" + "".join(f"{i},5;{i*2},25\n" for i in range(10)), "csv")
    _put(out, "csv/spaces_multi.txt", "x   y\n" + "".join(f"  {i}    {i*3}  \n" for i in range(10)), "csv")
    _put(out, "csv/quoted.csv", 'name,val\n"a,b",1\n"line\nbreak",2\n"esc ""q""",3\n"unterminated,4\n', "csv")
    _put(out, "csv/nan_inf_tokens.csv",
         "x,y\n1,nan\n2,NaN\n3,inf\n4,-inf\n5,Infinity\n6,1e400\n7,-1e400\n8,4.9e-324\n9,1e-400\n10,--\n11,\n",
         "csv")
    _put(out, "csv/scientific.csv", "x,y\n" + "".join(f"{i}e-3,{i}E+5\n" for i in range(1, 30)), "csv", "import-ok")
    _put(out, "csv/hex_and_junk.csv", "x,y\n0x10,1\n1_000,2\n1.2.3,3\n+-4,4\n.5,5\n5.,6\n", "csv")
    _put(out, "csv/datetime.csv",
         "t,v\n2026-01-01,1\n2026-01-02 12:34:56,2\n2026/01/03,3\n01/04/2026,4\n2026-13-40,5\n2026-01-05T06:07:08Z,6\n",
         "csv")
    _put(out, "csv/time_only.csv", "t,v\n00:00:01,1\n12:30,2\n23:59:59.999,3\n25:61:61,4\n", "csv")
    _put(out, "csv/japanese.csv", "温度,応力 (MPa),備考\n" + "".join(f"{i},{i*1.5},試料{i}\n" for i in range(10)), "csv", "import-ok")
    _put(out, "csv/shift_jis.csv", ("温度,応力\n" + "".join(f"{i},{i}\n" for i in range(5))).encode("shift_jis"), "csv")
    _put(out, "csv/invalid_utf8.csv", b"a,b\n\xff\xfe\xfd,1\n\xc3\x28,2\n\xed\xa0\x80,3\n", "csv")
    _put(out, "csv/nul_bytes.csv", b"a,b\n1\x00,2\n3,\x004\n", "csv")
    _put(out, "csv/comments.csv", "# comment\n# another\nx,y\n1,2\n# mid comment\n3,4\n", "csv")
    _put(out, "csv/units_row.csv", "x,y\nmm,N\n1,2\n3,4\n", "csv", "import-ok")
    _put(out, "csv/dup_headers.csv", "a,a,a,,\n1,2,3,4,5\n", "csv")
    _put(out, "csv/long_header.csv", ("h" * 100_000) + ",b\n1,2\n", "csv")
    _put(out, "csv/long_line_no_eol.csv", ",".join(str(i) for i in range(50_000)), "csv")
    _put(out, "csv/wide_2000_cols.csv",
         ",".join(f"c{i}" for i in range(2000)) + "\n" + ",".join(str(i) for i in range(2000)) + "\n", "csv")
    _put(out, "csv/tall_200k_rows.csv", "x,y\n" + "".join(f"{i},{(i*7919)%1000}\n" for i in range(200_000)), "csv",
         "import-ok")
    _put(out, "csv/single_cell.csv", "42", "csv")
    _put(out, "csv/single_col.csv", "v\n" + "".join(f"{i}\n" for i in range(30)), "csv", "import-ok")
    _put(out, "csv/only_delims.csv", ",,,,\n,,,,\n", "csv")
    _put(out, "csv/whitespace.csv", "   \n\t\t\n  ,  \n", "csv")
    _put(out, "csv/binary_garbage.bin", bytes(RNG.randrange(256) for _ in range(4096)), "csv")
    _put(out, "csv/numbers_with_text.csv", "x,y\n1,abc\n2,3\nfoo,4\n5,6e\n", "csv")
    _put(out, "csv/negative_zero.csv", "x,y\n-0,0\n-0.0,-0e0\n", "csv")
    _put(out, "csv/thousands_sep.csv", 'x,y\n"1,234",1\n"1.234,5",2\n1 234,3\n', "csv")
    _put(out, "csv/many_blank_rows.csv", "x,y\n" + "\n" * 5000 + "1,2\n", "csv")
    for name, data in (("sine", SERIES["sine"]), ("with_nan", SERIES["with_nan"]), ("huge", SERIES["huge"]),
                       ("subnormal", SERIES["subnormal"]), ("dup_x", SERIES["dup_x"])):
        txt = "x,y\n" + "".join(f"{i},{v!r}\n" for i, v in enumerate(data))
        _put(out, f"csv/series_{name}.csv", txt, "csv")


# ---------------------------------------------------------------------------
# XLSX (hand-built so malformed variants are possible)
# ---------------------------------------------------------------------------

CT = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
      '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
      '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
      '<Default Extension="xml" ContentType="application/xml"/>'
      '<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>'
      '{sheets}'
      '</Types>')
RELS = ('<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
        '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>'
        '</Relationships>')


def _xlsx(sheets: list[tuple[str, str]], shared: list[str] | None = None, styles: str | None = None,
          extra: dict[str, bytes] | None = None, drop: set[str] | None = None) -> bytes:
    drop = drop or set()
    wb = ('<?xml version="1.0" encoding="UTF-8"?><workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" '
          'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets>'
          + "".join(f'<sheet name="{n}" sheetId="{i+1}" r:id="rId{i+1}"/>' for i, (n, _) in enumerate(sheets))
          + '</sheets></workbook>')
    wbrels = ('<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
              + "".join(f'<Relationship Id="rId{i+1}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet{i+1}.xml"/>' for i in range(len(sheets)))
              + (f'<Relationship Id="rId{len(sheets)+1}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings" Target="sharedStrings.xml"/>' if shared is not None else "")
              + (f'<Relationship Id="rId{len(sheets)+2}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>' if styles is not None else "")
              + '</Relationships>')
    ovr = "".join(f'<Override PartName="/xl/worksheets/sheet{i+1}.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>' for i in range(len(sheets)))
    files = {
        "[Content_Types].xml": CT.format(sheets=ovr).encode(),
        "_rels/.rels": RELS.encode(),
        "xl/workbook.xml": wb.encode(),
        "xl/_rels/workbook.xml.rels": wbrels.encode(),
    }
    for i, (_, body) in enumerate(sheets):
        files[f"xl/worksheets/sheet{i+1}.xml"] = (
            '<?xml version="1.0" encoding="UTF-8"?><worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">'
            f'<sheetData>{body}</sheetData></worksheet>').encode()
    if shared is not None:
        files["xl/sharedStrings.xml"] = (
            '<?xml version="1.0" encoding="UTF-8"?><sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">'
            + "".join(f"<si><t>{s}</t></si>" for s in shared) + "</sst>").encode()
    if styles is not None:
        files["xl/styles.xml"] = styles.encode()
    files.update(extra or {})
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as z:
        for k, v in files.items():
            if k not in drop:
                z.writestr(k, v)
    return buf.getvalue()


def _row(r: int, cells: list[str]) -> str:
    return f'<row r="{r}">' + "".join(cells) + "</row>"


def _c(ref: str, v: str, t: str | None = None, s: str | None = None) -> str:
    a = f' r="{ref}"' if ref else ""
    a += f' t="{t}"' if t else ""
    a += f' s="{s}"' if s else ""
    return f"<c{a}><v>{v}</v></c>"


DATE_STYLES = ('<?xml version="1.0" encoding="UTF-8"?><styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">'
               '<numFmts count="1"><numFmt numFmtId="164" formatCode="yyyy-mm-dd hh:mm"/></numFmts>'
               '<cellXfs count="3"><xf numFmtId="0"/><xf numFmtId="14" applyNumberFormat="1"/><xf numFmtId="164" applyNumberFormat="1"/></cellXfs>'
               '</styleSheet>')


def gen_xlsx(out: Path) -> None:
    basic = _row(1, [_c("A1", "0", "s"), _c("B1", "1", "s")]) + "".join(
        _row(r, [_c(f"A{r}", str(r)), _c(f"B{r}", str(r * r * 0.5))]) for r in range(2, 30))
    _put(out, "xlsx/basic.xlsx", _xlsx([("Sheet1", basic)], shared=["x", "y"]), "xlsx", "import-ok")
    _put(out, "xlsx/two_sheets.xlsx", _xlsx([("A", basic), ("B", basic)], shared=["x", "y"]), "xlsx", "import-ok")
    _put(out, "xlsx/empty_sheet.xlsx", _xlsx([("Empty", "")]), "xlsx")
    _put(out, "xlsx/no_refs.xlsx", _xlsx([("S", _row(1, [_c("", "1"), _c("", "2")]) + "<row>" + _c("", "3") + "</row>")]), "xlsx")
    _put(out, "xlsx/absurd_refs.xlsx",
         _xlsx([("S", _row(1, [_c("XFD1048576", "1"), _c("A0", "2"), _c("a1", "3"), _c("ZZZZZ1", "4"),
                               _c("A99999999999", "5"), _c("1A", "6"), _c("A-1", "7")]))]), "xlsx")
    _put(out, "xlsx/far_cell.xlsx", _xlsx([("S", _row(100000, [_c("ZZ100000", "1")]))]), "xlsx")
    _put(out, "xlsx/bad_shared_index.xlsx",
         _xlsx([("S", _row(1, [_c("A1", "5", "s"), _c("B1", "-1", "s"), _c("C1", "abc", "s"), _c("D1", "1e30", "s")]))],
               shared=["only"]), "xlsx")
    _put(out, "xlsx/missing_shared.xlsx", _xlsx([("S", _row(1, [_c("A1", "0", "s")]))]), "xlsx")
    _put(out, "xlsx/inline_str.xlsx",
         _xlsx([("S", '<row r="1"><c r="A1" t="inlineStr"><is><t>hello</t></is></c><c r="B1" t="inlineStr"><is><r><t>ri</t></r><r><t>ch</t></r></is></c></row>'
                 + _row(2, [_c("A2", "1"), _c("B2", "2")]))]), "xlsx")
    _put(out, "xlsx/dates.xlsx",
         _xlsx([("S", _row(1, [_c("A1", "0", "s"), _c("B1", "1", "s")]) + "".join(
             _row(r, [_c(f"A{r}", str(45000 + r * 0.25), s="2"), _c(f"B{r}", str(r))]) for r in range(2, 12))
                 + _row(12, [_c("A12", "-5", s="1"), _c("B12", "1")]) + _row(13, [_c("A13", "1e12", s="1"), _c("B13", "1")]))],
               shared=["t", "v"], styles=DATE_STYLES), "xlsx")
    _put(out, "xlsx/bad_style_index.xlsx",
         _xlsx([("S", _row(1, [_c("A1", "1", s="99"), _c("B1", "2", s="-3")]))], styles=DATE_STYLES), "xlsx")
    _put(out, "xlsx/bool_err.xlsx",
         _xlsx([("S", _row(1, [_c("A1", "1", "b"), _c("B1", "#DIV/0!", "e"), _c("C1", "TRUE", "str"), _c("D1", "", "n")]))]), "xlsx")
    _put(out, "xlsx/nan_text_numbers.xlsx",
         _xlsx([("S", _row(1, [_c("A1", "NaN"), _c("B1", "inf"), _c("C1", "1e400"), _c("D1", "1,5")]))]), "xlsx")
    _put(out, "xlsx/weird_sheet_names.xlsx",
         _xlsx([("日本語 シート", basic), ("a/b\\c", basic), ("", basic), ("x" * 300, basic)], shared=["x", "y"]), "xlsx")
    _put(out, "xlsx/dup_sheet_names.xlsx", _xlsx([("S", basic), ("S", basic)], shared=["x", "y"]), "xlsx")
    _put(out, "xlsx/no_workbook.xlsx", _xlsx([("S", basic)], drop={"xl/workbook.xml"}), "xlsx")
    _put(out, "xlsx/no_sheet_part.xlsx", _xlsx([("S", basic)], drop={"xl/worksheets/sheet1.xml"}), "xlsx")
    _put(out, "xlsx/broken_xml.xlsx",
         _xlsx([("S", "<row r='1'><c r='A1'><v>1</v></c>")], extra={}), "xlsx")
    _put(out, "xlsx/wide.xlsx",
         _xlsx([("S", _row(1, [_c(f"{_col(i)}1", str(i)) for i in range(3000)]))]), "xlsx")
    big = "".join(_row(r, [_c(f"A{r}", str(r)), _c(f"B{r}", str(r % 97))]) for r in range(1, 60001))
    _put(out, "xlsx/tall.xlsx", _xlsx([("S", big)]), "xlsx", "import-ok")
    _put(out, "xlsx/not_a_zip.xlsx", b"PK\x03\x04" + bytes(200), "xlsx")
    _put(out, "xlsx/truncated.xlsx", _xlsx([("S", basic)], shared=["x", "y"])[:300], "xlsx")
    _put(out, "xlsx/zero_bytes.xlsx", b"", "xlsx")
    # zip bomb-ish: very compressible large sheet entry
    bomb = '<row r="1">' + ('<c r="A1"><v>1</v></c>' * 2_000_000) + "</row>"
    _put(out, "xlsx/repetitive_cells.xlsx", _xlsx([("S", bomb)]), "xlsx")
    # bit flips of a valid file
    seed = _xlsx([("S", basic)], shared=["x", "y"])
    for k in range(12):
        b = bytearray(seed)
        for _ in range(1 + k % 4):
            pos = RNG.randrange(len(b))
            b[pos] ^= 1 << RNG.randrange(8)
        _put(out, f"xlsx/bitflip_{k:02d}.xlsx", bytes(b), "xlsx")


def _col(i: int) -> str:
    s = ""
    i += 1
    while i:
        i, r = divmod(i - 1, 26)
        s = chr(65 + r) + s
    return s


# ---------------------------------------------------------------------------
# Projects (.sciprj): mutate the bundled sample projects
# ---------------------------------------------------------------------------

def gen_projects(out: Path) -> None:
    seeds = sorted((REPO / "test").glob("*.sciprj"))
    for seed in seeds:
        raw = seed.read_bytes()
        stem = seed.stem
        _put(out, f"project/{stem}.sciprj", raw, "project", "open-ok")
        for frac in (0.1, 0.5, 0.9, 0.99):
            _put(out, f"project/{stem}_trunc{int(frac*100)}.sciprj", raw[: int(len(raw) * frac)], "project")
        text = raw.decode("utf-8", "replace")
        subs = [
            ("rows_neg", lambda t: _re_sub(r'rows="\d+"', lambda m: 'rows="-5"', t)),
            ("rows_huge", lambda t: _re_sub(r'rows="\d+"', lambda m: 'rows="2147483647"', t)),
            ("rows_zero", lambda t: _re_sub(r'rows="\d+"', lambda m: 'rows="0"', t)),
            ("cols_neg", lambda t: _re_sub(r'columns="\d+"', lambda m: 'columns="-1"', t)),
            ("cols_more", lambda t: _re_sub(r'columns="\d+"', lambda m: 'columns="9"', t)),
            ("idx_neg", lambda t: _re_sub(r'index="\d+"', lambda m: 'index="-7"', t)),
            ("idx_huge", lambda t: _re_sub(r'index="\d+"', lambda m: 'index="99999999"', t)),
            ("idx_overflow", lambda t: _re_sub(r'index="\d+"', lambda m: 'index="99999999999999"', t)),
            ("cell_nan", lambda t: _re_sub(r'(invalid="no">)[-0-9.eE+]+', lambda m: m.group(1) + "nan", t)),
            ("cell_text", lambda t: _re_sub(r'(invalid="no">)[-0-9.eE+]+', lambda m: m.group(1) + "abc", t)),
            ("type_swap", lambda t: _re_sub(r'type="TypeDouble"', lambda m: 'type="TypeQDateTime"', t)),
            ("mode_bad", lambda t: _re_sub(r'mode="Numeric"', lambda m: 'mode="Bogus"', t)),
            ("windows_huge", lambda t: _re_sub(r"<windows>\t\d+", lambda m: "<windows>\t999999", t)),
            ("section_len_neg", lambda t: _re_sub(r"<table>\n\d+", lambda m: "<table>\n-50", t)),
            ("section_len_huge", lambda t: _re_sub(r"<table>\n\d+", lambda m: "<table>\n99999999", t)),
            ("noversion", lambda t: t.replace("SciDAVis", "XciDAVis", 1)),
            ("future_version", lambda t: _re_sub(r"project file", lambda m: "project file", t.replace("1.0.4", "99.99.99", 1))),
            ("dropclose", lambda t: t.replace("</table>", "", 1)),
            ("curve_missing_col", lambda t: _re_sub(r"(curve\t)([^\t]+)\t([^\t]+)", lambda m: m.group(1) + "Nope_x\tNope_y", t)),
            ("curve_short", lambda t: _re_sub(r"(curve\t[^\t]+\t[^\t]+)\t.*", lambda m: m.group(1), t)),
            ("curve_bad_ints", lambda t: _re_sub(r"(curve\t[^\t]+\t[^\t]+\t)([^\n]*)", lambda m: m.group(1) + "\t".join(["-2147483648"] * 20), t)),
            ("ggeometry_huge", lambda t: _re_sub(r"ggeometry\t[^\n]*", lambda m: "ggeometry\t0\t0\t2000000000\t2000000000", t)),
            ("ggeometry_neg", lambda t: _re_sub(r"ggeometry\t[^\n]*", lambda m: "ggeometry\t-5\t-5\t-100\t-100", t)),
            ("scale_nan", lambda t: _re_sub(r"(scale\t\d+\t)[^\n]*", lambda m: m.group(1) + "nan\tinf\t1\t0\t8\t5\t0\t0", t)),
            ("scale_inverted_log", lambda t: _re_sub(r"(scale\t\d+\t)[^\n]*", lambda m: m.group(1) + "10\t-5\t1\t1\t-8\t0\t1\t1", t)),
            ("scale_axis_oob", lambda t: _re_sub(r"scale\t\d+", lambda m: "scale\t77", t)),
            ("legend_frac", lambda t: _re_sub(r"<legend>\t[^\t]+\t[^\t]+", lambda m: "<legend>\tnan\t1e308", t)),
            ("font_size_bad", lambda t: _re_sub(r"(TitleFont\t[^\t]+\t)\d+", lambda m: m.group(1) + "-3", t)),
            ("margins_bad", lambda t: _re_sub(r"Margins\t[^\n]*", lambda m: "Margins\t-99999\tx\t\t", t)),
            ("layer_canvas_zero", lambda t: _re_sub(r"LayerCanvasSize\t[^\n]*", lambda m: "LayerCanvasSize\t0\t0", t)),
            ("grid_short", lambda t: _re_sub(r"grid\t[^\n]*", lambda m: "grid\t1", t)),
            ("enabled_axes_short", lambda t: _re_sub(r"EnabledAxes\t[^\n]*", lambda m: "EnabledAxes\t1", t)),
        ]
        for tag, fn in subs:
            mutated = fn(text)
            if mutated != text:
                _put(out, f"project/{stem}_{tag}.sciprj", mutated, "project")
        for k in range(6):
            b = bytearray(raw)
            for _ in range(8):
                pos = RNG.randrange(len(b))
                b[pos] = RNG.choice(b"\t\n<>-0129x\"")
            _put(out, f"project/{stem}_noise{k}.sciprj", bytes(b), "project")
    gen_project_line_sweep(out, seeds)
    _put(out, "project/empty.sciprj", b"", "project")
    _put(out, "project/header_only.sciprj", b"SciDAVis 0.2.4 project file\n", "project")
    _put(out, "project/garbage.sciprj", bytes(RNG.randrange(256) for _ in range(2048)), "project")


def gen_project_line_sweep(out: Path, seeds: list) -> None:
    """For every tab-separated line of the graph / 3D-plot sections: keep only its keyword
    (and one field), or set one numeric field to INT_MIN / a huge index.  The loaders index
    the split fields directly, so this finds every unchecked fList[n] / list.at(n)."""
    for seed in seeds:
        lines = seed.read_bytes().decode("utf-8", "replace").split("\n")
        in_win = False
        seen: set[str] = set()
        for i, ln in enumerate(lines):
            if ln.startswith(("<multiLayer>", "<SurfacePlot>", "<matrix>", "<note>")):
                in_win = True
            if not in_win or "\t" not in ln or ln.startswith("<table"):
                continue
            key = ln.split("\t", 1)[0].strip("<>") or "blank"
            if key in seen:
                continue
            seen.add(key)
            fields = ln.split("\t")
            safe = "".join(ch if ch.isalnum() else "_" for ch in key)[:24]
            variants = {"cut1": fields[0], "cut2": "\t".join(fields[:2])}
            for k, f in enumerate(fields[1:], 1):
                if f.lstrip("-").isdigit():
                    variants["intmin"] = "\t".join(fields[:k] + ["-2147483648"] + fields[k + 1:])
                    variants["big"] = "\t".join(fields[:k] + ["99999"] + fields[k + 1:])
                    break
            for tag, newln in variants.items():
                mutated = lines[:i] + [newln] + lines[i + 1:]
                _put(out, f"project/sweep_{seed.stem}_{safe}_{tag}.sciprj", "\n".join(mutated), "project")


def _re_sub(pat, repl, text):
    import re
    return re.sub(pat, repl, text, count=3, flags=re.DOTALL)


# ---------------------------------------------------------------------------
# Origin .opj / .opju
# ---------------------------------------------------------------------------

def gen_opj(out: Path) -> None:
    for seed in sorted((REPO / "test").glob("*.opj")):
        raw = seed.read_bytes()
        _put(out, f"opj/{seed.name}", raw, "opj", "import-ok")
        for frac in (0.05, 0.3, 0.7):
            _put(out, f"opj/{seed.stem}_trunc{int(frac*100)}.opj", raw[: int(len(raw) * frac)], "opj")
        for k in range(4):
            b = bytearray(raw)
            for _ in range(16):
                pos = RNG.randrange(len(b))
                b[pos] = RNG.randrange(256)
            _put(out, f"opj/{seed.stem}_flip{k}.opj", bytes(b), "opj")
    _put(out, "opj/empty.opju", b"", "opj")
    _put(out, "opj/garbage.opju", b"CPYUA 4.3 " + bytes(RNG.randrange(256) for _ in range(4096)), "opj")
    _put(out, "opj/fake_header.opj", b"CPYA 4.2673 552#\n" + struct.pack("<i", -1) * 64, "opj")


# ---------------------------------------------------------------------------

def gen_series(out: Path) -> None:
    for name, data in SERIES.items():
        _put(out, f"series/{name}.json", json.dumps([_json_safe(v) for v in data]), "series")


def main(argv: list[str]) -> int:
    out = Path(argv[1]) if len(argv) > 1 else REPO / "build-fuzz" / "corpus"
    out.mkdir(parents=True, exist_ok=True)
    gen_csv(out)
    gen_xlsx(out)
    gen_projects(out)
    gen_opj(out)
    gen_series(out)
    (out / "manifest.json").write_text(json.dumps(MANIFEST, indent=1, ensure_ascii=False), encoding="utf-8")
    kinds: dict[str, int] = {}
    for m in MANIFEST.values():
        kinds[m["kind"]] = kinds.get(m["kind"], 0) + 1
    print(f"wrote {len(MANIFEST)} files to {out}: {kinds}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
