#!/usr/bin/env python3
"""Compile-check single translation units without touching the shared build tree.

    python3 scripts/fuzz/syntax_check.py libscidavis/src/Foo.cpp [more.cpp ...]

Uses BUILD_DIR/compile_commands.json (default build-asan; configure with
-DCMAKE_EXPORT_COMPILE_COMMANDS=ON) and runs each command with -fsyntax-only, so several
agents can check their edits in parallel while one ninja owns the build directory.
"""

from __future__ import annotations

import json
import os
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main(argv: list[str]) -> int:
    build = Path(os.environ.get("BUILD_DIR", ROOT / "build-asan"))
    db = json.loads((build / "compile_commands.json").read_text())
    by_file = {str(Path(e["file"]).resolve()): e for e in db}
    rc = 0
    for f in argv[1:]:
        key = str((ROOT / f).resolve() if not os.path.isabs(f) else Path(f).resolve())
        e = by_file.get(key)
        if e is None:
            print(f"[skip] {f}: not in compile_commands.json")
            continue
        args = shlex.split(e["command"]) if "command" in e else list(e["arguments"])
        out: list[str] = []
        skip = False
        for a in args:
            if skip:
                skip = False
                continue
            if a in ("-o", "-MF", "-MT", "-MQ"):
                skip = True
                continue
            if a in ("-MD", "-MMD", "-c") or a == "ccache":
                continue
            out.append(a)
        out.append("-fsyntax-only")
        r = subprocess.run(out, cwd=e["directory"], capture_output=True, text=True)
        status = "ok" if r.returncode == 0 else "FAIL"
        print(f"[{status}] {f}")
        if r.returncode:
            rc = 1
            print(r.stderr[-6000:])
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
