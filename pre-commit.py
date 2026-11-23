#!/usr/bin/env python3
"""Git pre-commit hook for running clang-format on staged C/C++ files.

Works on both Linux/macOS and Windows (auto-discovers clang-format in PATH or
Visual Studio 2022 BuildTools/Community/Professional/Enterprise LLVM).
To install: cp pre-commit.py .git/hooks/pre-commit
"""
from __future__ import annotations

import pathlib
import shutil
import subprocess
import sys


def find_clang_format() -> str | None:
    exe = shutil.which("clang-format")
    if exe:
        return exe
    if sys.platform == "win32":
        for edition in ("BuildTools", "Community", "Professional", "Enterprise"):
            for prog in (r"C:\Program Files (x86)", r"C:\Program Files"):
                cand = (
                    pathlib.Path(prog)
                    / "Microsoft Visual Studio"
                    / "2022"
                    / edition
                    / "VC"
                    / "Tools"
                    / "Llvm"
                    / "x64"
                    / "bin"
                    / "clang-format.exe"
                )
                if cand.is_file():
                    return str(cand)
    return None


def main() -> int:
    clang_fmt = find_clang_format()
    if not clang_fmt:
        print("[WARN] clang-format not found; skipping format check.", file=sys.stderr)
        return 0

    root = pathlib.Path(
        subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip()
    )
    staged = subprocess.check_output(
        ["git", "diff-index", "--cached", "--name-only", "--diff-filter=ACMR", "HEAD"],
        cwd=root,
        text=True,
    ).splitlines()

    reformatted = 0
    for rel in staged:
        rel = rel.strip()
        if not rel.endswith((".cpp", ".h")):
            continue
        if rel.startswith("3rdparty/"):
            continue
        fpath = root / rel
        if not fpath.is_file():
            continue
        original = fpath.read_bytes()
        formatted = subprocess.check_output([clang_fmt, str(fpath)])
        if formatted != original:
            fpath.write_bytes(formatted)
            print(f"{rel} was reformatted by clang-format; please stage and commit again.")
            reformatted += 1

    return 1 if reformatted else 0


if __name__ == "__main__":
    raise SystemExit(main())
