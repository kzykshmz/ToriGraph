#!/usr/bin/env python3
"""ToriGraph の日本語辞書 (libscidavis/src/i18n/ja_dictionary.tsv) の点検と用語統一.

使い方:
    python scripts/check_ja_glossary.py            # 点検だけ (CI 用。違反があれば終了コード 1)
    python scripts/check_ja_glossary.py --fix      # 用語集の機械置換を辞書に書き戻す
    python scripts/check_ja_glossary.py --sources  # C++ 側の registerExtraTranslation の訳も一覧する (参考表示)

辞書の書式 (UTF-8):
    英語原文 <TAB> 日本語訳 [<TAB> 文脈]
    - 文脈は tr() のクラス名 (QObject の context)。空なら全文脈で使う既定訳。
      文脈つきの行は、その文脈では既定訳より優先される (I18nJapanese.cpp の二段引き)。
    - エスケープ: \\n 改行, \\t タブ, \\r 復帰, \\\\ バックスラッシュ, \\s 空白 (欄の先頭・末尾の空白に使う)
    - '#' で始まる行と空行は読み飛ばす。

用語集 (ToriGraph 改善レビュー H3 / 研究室配布版の統一訳):
    レイヤー、目盛り、シンボル、エラーバー、ワークシート、行列、フィット、軸タイトル、
    エクスポート、平滑化、不透明度、インスペクタ、物理量 / 単位 (ISIJ)
    括弧とコロンは半角、ボタンの「OK」は「OK」だけ、訳に「&&」を残さない。

C++ 側 (I18nJapanese.cpp の normalizeJapaneseGlossary) は、コードから登録される訳に
同じ規則のうち文脈に依らない部分 (RUNTIME_SAFE=True の規則) を実行時に適用する。
規則を変えるときは両方を合わせること。
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from dataclasses import dataclass
from typing import Callable, List, Optional, Tuple

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TSV_PATH = os.path.join(ROOT, "libscidavis", "src", "i18n", "ja_dictionary.tsv")


# ---------------------------------------------------------------------------
# TSV の読み書き
# ---------------------------------------------------------------------------

def unescape(field: str) -> str:
    out = []
    i = 0
    while i < len(field):
        c = field[i]
        if c == "\\" and i + 1 < len(field):
            n = field[i + 1]
            out.append({"n": "\n", "t": "\t", "r": "\r", "\\": "\\", "s": " "}.get(n, "\\" + n))
            i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


def escape(text: str) -> str:
    s = (text.replace("\\", "\\\\").replace("\n", "\\n").replace("\t", "\\t")
         .replace("\r", "\\r"))
    # 先頭・末尾の空白は見えにくく、エディタで消えやすいので \s にする
    lead = len(s) - len(s.lstrip(" "))
    s = "\\s" * lead + s[lead:]
    trail = len(s) - len(s.rstrip(" "))
    if trail:
        s = s[: len(s) - trail] + "\\s" * trail
    return s


@dataclass
class Entry:
    en: str
    ja: str
    context: str = ""
    line: int = 0


def read_tsv(path: str = TSV_PATH) -> Tuple[List[str], List[Entry]]:
    """(ヘッダのコメント行, 項目) を返す。"""
    header: List[str] = []
    entries: List[Entry] = []
    with open(path, "r", encoding="utf-8", newline="") as fh:
        for no, raw in enumerate(fh, 1):
            line = raw.rstrip("\r\n")
            if not line or line.startswith("#"):
                if not entries:
                    header.append(line)
                continue
            parts = line.split("\t")
            if len(parts) not in (2, 3):
                raise ValueError(f"{path}:{no}: 欄の数が {len(parts)} です (2 か 3)")
            ctx = parts[2] if len(parts) == 3 else ""
            entries.append(Entry(unescape(parts[0]), unescape(parts[1]), ctx, no))
    return header, entries


def write_tsv(header: List[str], entries: List[Entry], path: str = TSV_PATH) -> None:
    with open(path, "w", encoding="utf-8", newline="") as fh:
        for h in header:
            fh.write(h + "\r\n")
        for e in entries:
            cols = [escape(e.en), escape(e.ja)]
            if e.context:
                cols.append(e.context)
            fh.write("\t".join(cols) + "\r\n")


# ---------------------------------------------------------------------------
# 用語集の規則
# ---------------------------------------------------------------------------

@dataclass
class Rule:
    rid: str
    pattern: str
    replacement: Optional[str | Callable[[re.Match], str]]
    description: str
    en_only: Optional[str] = None      # 英語原文がこれに合うときだけ適用
    en_except: Optional[str] = None    # 英語原文がこれに合うときは適用しない
    runtime_safe: bool = False         # C++ の実行時正規化と同じ規則か

    def applies(self, en: str) -> bool:
        if self.en_only and not re.search(self.en_only, en, re.I):
            return False
        if self.en_except and re.search(self.en_except, en, re.I):
            return False
        return True


SYMBOL_PLOT_EN = (r"Symbol (Shape|Size|Edge|Fill|Skip|Color|Opacity|&)|Symbol Shapes|"
                  r"Line ?\+ ?Sym|No Symbol|Colors && Symbols|symbols, frame|symbol styles|"
                  r"Style Scheme")

# 短いラベルで英語をそのまま括弧で繰り返しているもの。手法名・関数名 (Gaussian,
# Savitzky-Golay など) は文献や Origin の用語と照合しやすいよう残し、ここに挙げた
# 画面部品の語だけ英語併記を外す (英語は英語 UI に切り替えれば見られる)。
ECHO_STRIP_EN = {
    "black", "blue", "cyan", "dark cyan", "dark gray", "dark yellow", "gray", "green",
    "light cyan", "light gray", "light magenta", "light yellow", "magenta", "navy", "olive",
    "orange", "pink", "purple", "red", "royal", "violet", "white", "wine", "yellow",
    "linear", "left", "right", "top", "bottom", "flat", "round", "square", "solid", "bevel",
    "miter", "bdiagonal", "fdiagonal", "cross", "diagcross", "diamond", "rectangle", "cone",
    "slope", "intercept", "real", "imaginary", "median", "cross hair", "intersect",
    "upper envelope", "lower envelope", "both (upper & lower)", "before column:",
    "after column:", "peak analyzer goal", "peak analyzer goal:", "dissimilarity height",
    "box plot", "pareto chart", "ternary plot", "autoexec", "average",
}


def _strip_echo(en: str, ja: str) -> str:
    key = en.replace("&", "").strip().lower()
    if key not in ECHO_STRIP_EN:
        return ja
    return re.sub(r"\s?\((?:[A-Z][A-Za-z\- ]*|Both|Before|After|Goal|Height|Pareto|Ternary)\)", "",
                  ja, count=1)


RULES: List[Rule] = [
    Rule("paren-full", "[（）]", lambda m: "(" if m.group(0) == "（" else ")",
         "括弧は半角 ( ) にする", runtime_safe=True),
    Rule("colon-full", "：", ":", "コロンは半角にする", runtime_safe=True),
    Rule("ok-kettei", r"OK \(決定\)", "OK", "「OK (決定)」は「OK」", runtime_safe=True),
    Rule("layer", "レイヤ(?!ー)", "レイヤー", "レイヤ → レイヤー", runtime_safe=True),
    Rule("tick", "目盛(?!り)", "目盛り", "目盛 → 目盛り", runtime_safe=True),
    Rule("errorbar", "誤差棒|誤差バー", "エラーバー", "誤差棒 → エラーバー", runtime_safe=True),
    Rule("fit", "フィッティング", "フィット", "フィッティング → フィット", runtime_safe=True),
    Rule("smooth-dup", r"スムージング \(平滑化\) ?", "平滑化", "スムージング (平滑化) → 平滑化",
         runtime_safe=True),
    Rule("smooth", "スムージング", "平滑化", "スムージング → 平滑化", runtime_safe=True),
    Rule("inspector", "インスペクター", "インスペクタ", "インスペクター → インスペクタ",
         runtime_safe=True),
    Rule("matrix", "マトリッ?クス", "行列", "マトリクス → 行列", runtime_safe=True),
    Rule("export", "エキスポート", "エクスポート", "エキスポート → エクスポート", runtime_safe=True),
    Rule("axis-title", "軸ラベル|軸題(?!名)", "軸タイトル", "軸ラベル・軸題 → 軸タイトル",
         en_except=r"Category", runtime_safe=False),
    Rule("worksheet", "テーブル", "ワークシート", "Table → ワークシート",
         en_except=r"TeX|ASCII|caption|Table labe?l?s|^Table1$|element", runtime_safe=False),
    Rule("symbol", "(?<!特殊)記号", "シンボル", "プロットの Symbol → シンボル",
         en_only=SYMBOL_PLOT_EN, runtime_safe=False),
    Rule("double-amp", r"\s*&&\s*", "・", "訳に && を残さない (・ でつなぐ)", runtime_safe=True),
    Rule("opacity", "(?<!不)透明度", "不透明度", "Opacity → 不透明度",
         en_only=r"Opacity", runtime_safe=False),
    Rule("slide", "プレゼン発表スライド|プレゼンスライド", "発表スライド",
         "Presentation Slide → 発表スライド", runtime_safe=True),
    Rule("isij-unit", r"物理量 / 単位 \(ISIJ形式\)", "物理量 / 単位 (ISIJ)",
         "物理量 / 単位 (ISIJ) の表記", runtime_safe=False),
]


def apply_rules(en: str, ja: str) -> Tuple[str, List[str]]:
    hits = []
    out = ja
    for r in RULES:
        if not r.applies(en):
            continue
        new = re.sub(r.pattern, r.replacement, out)
        if new != out:
            hits.append(r.rid)
            out = new
    new = _strip_echo(en, out)
    if new != out:
        hits.append("english-echo")
        out = new
    return out, hits


# ---------------------------------------------------------------------------
# 点検
# ---------------------------------------------------------------------------

def check_entries(entries: List[Entry]) -> List[str]:
    problems: List[str] = []
    seen = {}
    for e in entries:
        key = (e.context, e.en)
        if key in seen:
            problems.append(f"line {e.line}: 重複キー {e.en!r} (文脈 {e.context or '既定'}、"
                            f"先の行 {seen[key]})")
        else:
            seen[key] = e.line
        if not e.ja:
            problems.append(f"line {e.line}: 訳が空 {e.en!r}")
        fixed, hits = apply_rules(e.en, e.ja)
        if hits:
            problems.append(f"line {e.line}: 用語の揺れ [{', '.join(hits)}] {e.en!r}: "
                            f"{e.ja!r} → {fixed!r}")
        # 英語のプレースホルダ (%1 など) が訳で欠けていないか
        for ph in sorted(set(re.findall(r"%\d", e.en))):
            if ph not in e.ja:
                problems.append(f"line {e.line}: プレースホルダ {ph} が訳に無い {e.en!r}")
        if e.context and not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_:]*", e.context):
            problems.append(f"line {e.line}: 文脈名が不正 {e.context!r}")
    return problems


SRC_PATTERN = re.compile(
    r'(?:\badd|\breg|registerExtraTranslation)\(\s*(?:QString::fromUtf8\()?"((?:[^"\\]|\\.)*)"\)?\s*,'
    r'\s*(?:QString::fromUtf8\()?"((?:[^"\\]|\\.)*)"')


def scan_sources() -> List[str]:
    """C++ に直書きされた訳 (実行時に正規化される) のうち、規則に合わないもの。"""
    notes = []
    src_dir = os.path.join(ROOT, "libscidavis", "src")
    for name in sorted(os.listdir(src_dir)):
        if not name.endswith(".cpp") or name == "I18nJapanese.cpp":
            continue
        path = os.path.join(src_dir, name)
        with open(path, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
        for m in SRC_PATTERN.finditer(text):
            en, ja = m.group(1), m.group(2)
            fixed, hits = apply_rules(en, ja)
            hits = [h for h in hits if h != "english-echo"]
            if hits:
                line = text.count("\n", 0, m.start()) + 1
                notes.append(f"{name}:{line}: [{', '.join(hits)}] {en!r}: {ja!r} → {fixed!r}")
    return notes


def main(argv: List[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--fix", action="store_true", help="用語集の機械置換を辞書へ書き戻す")
    ap.add_argument("--sources", action="store_true",
                    help="C++ に直書きされた訳も点検して表示する (終了コードには含めない)")
    ap.add_argument("--tsv", default=TSV_PATH)
    args = ap.parse_args(argv)

    if args.fix:
        # 行単位で訳の欄だけを書き換える (コメント行・並び順・改行コードはそのまま)
        with open(args.tsv, "r", encoding="utf-8", newline="") as fh:
            raw_lines = fh.readlines()
        changed = 0
        out_lines = []
        for raw in raw_lines:
            body = raw.rstrip("\r\n")
            eol = raw[len(body):]
            if body and not body.startswith("#"):
                parts = body.split("\t")
                if len(parts) in (2, 3):
                    en, ja = unescape(parts[0]), unescape(parts[1])
                    new, _ = apply_rules(en, ja)
                    if new != ja:
                        parts[1] = escape(new)
                        body = "\t".join(parts)
                        changed += 1
            out_lines.append(body + eol)
        with open(args.tsv, "w", encoding="utf-8", newline="") as fh:
            fh.writelines(out_lines)
        print(f"{changed} 件を書き換えました: {args.tsv}")
    header, entries = read_tsv(args.tsv)

    problems = check_entries(entries)
    for p in problems:
        print(p)
    if args.sources:
        notes = scan_sources()
        if notes:
            print(f"\n参考: C++ 直書きの訳で規則に合わないもの {len(notes)} 件"
                  "(runtime_safe の規則は実行時に正規化される)")
            for n in notes:
                print("  " + n)
    print(f"{len(entries)} 項目を点検、問題 {len(problems)} 件")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
