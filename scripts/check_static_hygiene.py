#!/usr/bin/env python3
"""Fast static hygiene scanner for recurring ToriGraph C++/Qt pitfalls.

Checks:
  1. Bare `QSettings var;` / `QSettings var()` outside central settings helpers.
  2. `QLocale::OmitGroupSeparator` used for `.toDouble()` parsing instead of `RejectGroupSeparator`.
  3. Unscoped container/popup `setStyleSheet("...background...")` without a CSS selector block.
  4. Single unescaped ` & ` inside `tr("...")` passed to mnemonic-parsing Qt widgets/actions.
  5. `new QShortcut(...Ctrl+A...)` missing `Qt::WidgetWithChildrenShortcut`.
"""

from __future__ import annotations

import argparse
import pathlib
import re
from dataclasses import dataclass
from typing import Iterable, List

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC_DIRS = [ROOT / "libscidavis" / "src", ROOT / "scidavis" / "src"]

QSETTINGS_ALLOW_FILES = {
    "ApplicationWindow.cpp",
    "ApplicationWindow_SettingsAndStyles.cpp",
    "SettingsTransfer.cpp",
    "OleServer.cpp",
}

NON_MNEMONIC_CALL_RE = re.compile(
    r"\b(?:setToolTip|setStatusTip|setWhatsThis|setAccessibleName|setAccessibleDescription|"
    r"setPlaceholderText|setWindowTitle|QLabel)\b"
)
EXPLICIT_MNEMONIC_RE = re.compile(
    r"(?:addAction\s*\(|new\s+Q(?:Action|PushButton|CheckBox|RadioButton|GroupBox|ToolButton)\s*\(|"
    r"->\s*(?:addRow|setTitle)\s*\()"
)
ADDROW_LAYOUT_ARG_RE = re.compile(
    r"^(?:row\w*|[a-z]\w*Row(?:Lay)?|[A-Za-z0-9_]*Layout\w*|"
    r"(?:[hv]|btn|box)?[Ll]ay(?:out)?\w*|new\s+Q[A-Za-z]*Layout\b)"
)
LABEL_VAR_RE = re.compile(r"(?:lbl|label|hint|cue|badge|pill|preview)", re.IGNORECASE)
SINGLE_AMP_TR_RE = re.compile(r'\btr(?:Utf8)?\s*\(\s*"([^"\n]*(?<!&) & (?!&)[^"\n]*)"')
BARE_QSETTINGS_RE = re.compile(r"^\s*QSettings\s+(?![&*])([A-Za-z_]\w*)\s*(?:;|\(\s*\)\s*;)")
SHORTCUT_CTRL_A_RE = re.compile(r"new\s+QShortcut\s*\([^;]*\bCtrl\+A\b[^;]*\)", re.DOTALL)


@dataclass
class Finding:
    rule: str
    path: str
    line: int
    detail: str


def _iter_cpp_files() -> Iterable[pathlib.Path]:
    for d in SRC_DIRS:
        if not d.is_dir():
            continue
        for p in sorted(d.rglob("*")):
            if p.suffix not in {".cpp", ".h"}:
                continue
            if "3rdparty" in p.parts:
                continue
            yield p


def _strip_comments(line: str, in_block: bool) -> tuple[str, bool]:
    out: List[str] = []
    i = 0
    n = len(line)
    in_str = False
    while i < n:
        if in_block:
            end = line.find("*/", i)
            if end < 0:
                return "".join(out), True
            i = end + 2
            in_block = False
            continue
        ch = line[i]
        if ch == '"' and (i == 0 or line[i - 1] != "\\"):
            in_str = not in_str
            out.append(ch)
            i += 1
        elif not in_str and line.startswith("/*", i):
            in_block = True
            i += 2
        elif not in_str and line.startswith("//", i):
            break
        else:
            out.append(ch)
            i += 1
    return "".join(out), in_block


def _extract_stmt(clean_lines: List[str], start_idx: int) -> str:
    n_lines = len(clean_lines)
    end_idx = start_idx
    while end_idx + 1 < n_lines and ";" not in clean_lines[end_idx]:
        if end_idx - start_idx >= 6:
            break
        end_idx += 1
    begin_idx = start_idx
    while begin_idx > 0 and begin_idx > start_idx - 3:
        prev = clean_lines[begin_idx - 1].rstrip()
        if not prev or prev.endswith(";") or prev.endswith("{") or prev.endswith("}"):
            break
        begin_idx -= 1
    return " ".join(clean_lines[begin_idx : end_idx + 1])


def _parse_addrow_args(stmt: str) -> List[str] | None:
    m = re.search(r"->\s*addRow\s*\(", stmt)
    if not m:
        return None
    i = m.end()
    depth = 1
    in_str = False
    args: List[str] = []
    cur: List[str] = []
    while i < len(stmt) and depth > 0:
        ch = stmt[i]
        if ch == '"' and (i == 0 or stmt[i - 1] != "\\"):
            in_str = not in_str
            cur.append(ch)
        elif not in_str:
            if ch == "(":
                depth += 1
                cur.append(ch)
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    args.append("".join(cur).strip())
                    break
                cur.append(ch)
            elif ch == "," and depth == 1:
                args.append("".join(cur).strip())
                cur = []
            else:
                cur.append(ch)
        else:
            cur.append(ch)
        i += 1
    return args if len(args) == 2 else None


def scan_file(path: pathlib.Path) -> List[Finding]:
    rel = str(path.relative_to(ROOT)).replace("\\", "/")
    text = path.read_text(encoding="utf-8", errors="replace")
    raw_lines = text.splitlines()
    clean_lines: List[str] = []
    in_block = False
    for ln in raw_lines:
        c, in_block = _strip_comments(ln, in_block)
        clean_lines.append(c)

    findings: List[Finding] = []
    n_lines = len(clean_lines)

    # Rule 1: Bare QSettings instances (no custom path/IniFormat args)
    if path.name not in QSETTINGS_ALLOW_FILES:
        for idx, line in enumerate(clean_lines, 1):
            if BARE_QSETTINGS_RE.match(line):
                findings.append(
                    Finding(
                        "bare-qsettings",
                        rel,
                        idx,
                        "Use ApplicationWindow::getSettings() instead of bare QSettings",
                    )
                )

    # Rule 2: OmitGroupSeparator used on a locale variable whose .toDouble() lacks RejectGroupSeparator
    omit_var_re = re.compile(r"([A-Za-z_]\w*)\.setNumberOptions\([^;]*OmitGroupSeparator")
    for idx, line in enumerate(clean_lines, 1):
        om = omit_var_re.search(line)
        if not om:
            continue
        var = om.group(1)
        win = "\n".join(clean_lines[max(0, idx - 1) : min(n_lines, idx + 8)])
        if re.search(rf"\b{re.escape(var)}\.toDouble\(", win) and "RejectGroupSeparator" not in win:
            findings.append(
                Finding(
                    "omit-group-sep-parse",
                    rel,
                    idx,
                    f"QLocale::OmitGroupSeparator on '{var}' affects formatting only; use RejectGroupSeparator for toDouble()",
                )
            )

    # Rule 3: Unscoped setStyleSheet containing background property on container/non-label widgets
    for idx in range(n_lines):
        line = clean_lines[idx]
        if "setStyleSheet(" not in line:
            continue
        stmt = _extract_stmt(clean_lines, idx)
        m = re.search(
            r'([A-Za-z0-9_>.\s\(\)]*)->setStyleSheet\(\s*(?:QStringLiteral|QLatin1String)?\s*\(?\s*"([^"]*)"',
            stmt,
        )
        if not m:
            continue
        caller, css = m.group(1), m.group(2)
        if LABEL_VAR_RE.search(caller):
            continue
        if re.search(r"\bbackground(?:-color)?\s*:", css) and "{" not in css:
            findings.append(
                Finding(
                    "unscoped-stylesheet-bg",
                    rel,
                    idx + 1,
                    f"Unscoped background stylesheet can propagate to child popups/QToolTip: {css[:60]!r}",
                )
            )

    # Rule 4: Single ' & ' in tr() passed to mnemonic-parsing widget/action calls
    for idx in range(n_lines):
        line = clean_lines[idx]
        m = SINGLE_AMP_TR_RE.search(line)
        if not m:
            continue
        stmt = _extract_stmt(clean_lines, idx)
        if NON_MNEMONIC_CALL_RE.search(stmt):
            continue
        # QFormLayout::addRow(QString, QLayout*) creates a buddy-less QLabel, so '&' is literal
        addrow_args = _parse_addrow_args(stmt)
        if addrow_args and ADDROW_LAYOUT_ARG_RE.match(addrow_args[1]):
            continue
        is_explicit = bool(EXPLICIT_MNEMONIC_RE.search(stmt))
        is_btn_settext = bool(
            re.search(r"([A-Za-z0-9_>]+)->\s*setText\s*\(", stmt)
            and not LABEL_VAR_RE.search(re.search(r"([A-Za-z0-9_>]+)->\s*setText\s*\(", stmt).group(1))
        )
        if is_explicit or is_btn_settext:
            findings.append(
                Finding(
                    "unescaped-ampersand",
                    rel,
                    idx + 1,
                    f"Escape literal ' & ' as ' && ' in mnemonic widget label: {m.group(1)!r}",
                )
            )

    # Rule 5: QShortcut("Ctrl+A") missing Qt::WidgetWithChildrenShortcut
    joined = "\n".join(clean_lines)
    for m in SHORTCUT_CTRL_A_RE.finditer(joined):
        snippet = m.group(0)
        if "WidgetWithChildrenShortcut" not in snippet and "WidgetShortcut" not in snippet:
            line_no = joined[: m.start()].count("\n") + 1
            findings.append(
                Finding(
                    "ctrl-a-shortcut-scope",
                    rel,
                    line_no,
                    "Pass Qt::WidgetWithChildrenShortcut to Ctrl+A QShortcut to avoid multi-window ambiguity",
                )
            )

    # Rule 6: Literal '&&' in buddy-less QFormLayout::addRow(QString, QLayout*)
    for idx in range(n_lines):
        if "addRow(" not in clean_lines[idx]:
            continue
        stmt = _extract_stmt(clean_lines, idx)
        addrow_args = _parse_addrow_args(stmt)
        if (
            addrow_args
            and ADDROW_LAYOUT_ARG_RE.match(addrow_args[1])
            and re.search(r'"[^"\n]*&&[^"\n]*"', addrow_args[0])
        ):
            findings.append(
                Finding(
                    "addrow-layout-double-amp",
                    rel,
                    idx + 1,
                    f"Use single '&' (no buddy set) in addRow(label, layout): {addrow_args[0][:60]!r}",
                )
            )

    return findings


def main() -> int:
    parser = argparse.ArgumentParser(description="Check ToriGraph C++ sources for recurring static pitfalls.")
    parser.add_argument("--warn-only", action="store_true", help="Print findings without failing (exit 0).")
    args = parser.parse_args()

    all_findings: List[Finding] = []
    for path in _iter_cpp_files():
        all_findings.extend(scan_file(path))

    if not all_findings:
        print("Static hygiene check: 0 issue(s) found.")
        return 0

    for f in all_findings:
        tag = "WARN" if args.warn_only else "FAIL"
        print(f"[{tag}][{f.rule}] {f.path}:{f.line}: {f.detail}")

    print(f"Static hygiene check: {len(all_findings)} issue(s) found.")
    return 0 if args.warn_only else 1


if __name__ == "__main__":
    raise SystemExit(main())
