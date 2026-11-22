#!/usr/bin/env python3
"""Check that every ToriGraph version string agrees with the CMake project VERSION.

The top-level ``CMakeLists.txt`` ``project(... VERSION x.y.z ...)`` is the single
source of truth.  ``libscidavis/src/version.cpp`` and the Linux appdata file are
generated from it by CMake, the Windows installer receives it from
``scripts/build-windows-installer.ps1``, and the Python SDK keeps a literal
``__version__`` that must match (``pyproject.toml`` reads it dynamically).

Usage::

    python scripts/check_version.py                 # consistency check only
    python scripts/check_version.py --print         # print the version (after checking)
    python scripts/check_version.py --tag v3.0.0    # also require the git tag to match

Exit status is 0 when everything agrees and 1 otherwise.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent


def _read(rel: str) -> str:
    return (REPO / rel).read_text(encoding="utf-8-sig")


def cmake_version() -> str:
    text = _read("CMakeLists.txt")
    m = re.search(r"project\s*\(\s*scidavis\b[^)]*?\bVERSION\s+([0-9]+\.[0-9]+\.[0-9]+)", text, re.S)
    if not m:
        raise SystemExit("check_version: could not find project(... VERSION x.y.z) in CMakeLists.txt")
    return m.group(1)


def cmake_release_date() -> str:
    m = re.search(r'set\(\s*TORIGRAPH_RELEASE_DATE\s+"([^"]*)"\s*\)', _read("CMakeLists.txt"))
    return m.group(1) if m else ""


def python_version() -> str:
    m = re.search(r'^__version__\s*=\s*["\']([^"\']+)["\']', _read("python/torigraph/__init__.py"), re.M)
    return m.group(1) if m else ""


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--tag", default="", help="git tag (e.g. v3.0.0 or refs/tags/v3.0.0) that must match")
    ap.add_argument("--print", dest="print_version", action="store_true", help="print the version")
    args = ap.parse_args(argv)

    errors: list[str] = []
    ver = cmake_version()

    date = cmake_release_date()
    if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", date):
        errors.append(f"TORIGRAPH_RELEASE_DATE in CMakeLists.txt must be YYYY-MM-DD (found {date!r})")

    py_ver = python_version()
    if py_ver != ver:
        errors.append(f"python/torigraph/__init__.py __version__ is {py_ver!r}, expected {ver!r}")

    pyproject = _read("python/pyproject.toml")
    project_section = re.search(r"^\[project\](.*?)(?=^\[)", pyproject, re.S | re.M)
    if project_section and re.search(r"^\s*version\s*=", project_section.group(1), re.M):
        errors.append("python/pyproject.toml must not hard-code [project] version (use dynamic = [\"version\"])")
    if 'attr = "torigraph.__version__"' not in pyproject:
        errors.append("python/pyproject.toml must read the version from torigraph.__version__")

    iss = _read("installer/ToriGraph.iss")
    if re.search(r'#define\s+MyAppVersion\s+"', iss):
        errors.append("installer/ToriGraph.iss must not hard-code MyAppVersion (pass /DMyAppVersion=...)")

    if (REPO / "libscidavis/src/version.cpp").exists():
        errors.append("libscidavis/src/version.cpp must not exist; it is generated from version.cpp.in")

    if args.tag:
        tag = args.tag.strip()
        tag = tag[len("refs/tags/"):] if tag.startswith("refs/tags/") else tag
        tag_ver = tag[1:] if tag[:1] in ("v", "V") else tag
        if tag_ver != ver:
            errors.append(
                f"git tag {args.tag!r} does not match CMake project VERSION {ver!r}; "
                "bump VERSION (and __version__) before tagging, or re-tag"
            )

    if errors:
        for e in errors:
            print(f"check_version: ERROR: {e}", file=sys.stderr)
        return 1
    if args.print_version:
        print(ver)
    else:
        print(f"check_version: OK ({ver}, released {date})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
