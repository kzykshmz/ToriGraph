"""Shared pytest fixtures for the ToriGraph Python SDK tests.

* Tests that only exercise the SDK (protocol, version checks) need nothing else.
* Tests marked ``app`` drive a real ToriGraph process.  They share ONE headless
  session (``tg_app``, session scope) so the application starts only once.  They
  are skipped when no ToriGraph executable is found (set ``TORIGRAPH_EXE``).
* ``slow`` marks long end-to-end gates; CI runs ``pytest -m "not slow"``.
"""

from __future__ import annotations

import sys
from pathlib import Path
from typing import Any, Dict, Iterator

import pytest

TESTS_DIR = Path(__file__).resolve().parent
SDK_DIR = TESTS_DIR.parent
REPO_ROOT = SDK_DIR.parent
for p in (str(SDK_DIR), str(TESTS_DIR)):
    if p not in sys.path:
        sys.path.insert(0, p)

import torigraph as tg  # noqa: E402
from torigraph.client import _discover_torigraph_executable  # noqa: E402


def _find_exe() -> Path | None:
    try:
        return _discover_torigraph_executable()
    except tg.ToriGraphError:
        return None


def pytest_collection_modifyitems(config: pytest.Config, items: list) -> None:
    app_fixtures = {"tg_app", "torigraph_exe", "atlas_context"}
    for item in items:
        if app_fixtures.intersection(getattr(item, "fixturenames", ())):
            item.add_marker(pytest.mark.app)


@pytest.fixture(scope="session")
def torigraph_exe() -> Path:
    exe = _find_exe()
    if exe is None:
        pytest.skip("ToriGraph executable not found (build it or set TORIGRAPH_EXE)")
    return exe


@pytest.fixture(scope="session")
def tg_app(torigraph_exe: Path) -> Iterator[tg.Session]:
    """One headless Japanese-UI ToriGraph session shared by every ``app`` test."""
    with tg.launch(exe_path=torigraph_exe, headless=True, language="ja") as app:
        yield app


@pytest.fixture(scope="session")
def e2e_tmp_dir(tmp_path_factory: pytest.TempPathFactory) -> Path:
    return tmp_path_factory.mktemp("torigraph_e2e")


@pytest.fixture(scope="session")
def e2e_refs() -> Dict[str, Any]:
    """Window names produced by earlier workflows and reused by later ones."""
    return {}
