"""pytest entry points for the 30-figure Materials-Science Atlas & agent gate.

The four personas render and audit 30 publication figures (600 dpi, EMF+, PPTX)
and start extra ToriGraph processes, so the whole module is marked ``slow``.
``python tests/e2e_matsci_atlas.py`` runs the same gate as a script.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any, Dict, Iterator

import pytest

import e2e_matsci_atlas as atlas

pytestmark = pytest.mark.slow


@pytest.fixture(scope="module")
def atlas_context(torigraph_exe: Path, tmp_path_factory: pytest.TempPathFactory) -> Iterator[Dict[str, Any]]:
    out_dir = tmp_path_factory.mktemp("torigraph_atlas")
    launcher = atlas.PortPoolLauncher(atlas.PORT_POOL)
    reg = atlas.AtlasRegistry(out_dir)
    lab_files = atlas._write_synthetic_lab_files(out_dir / "synthetic_lab_inputs")
    try:
        with launcher.session(language="en") as (app, _port, _pid):
            yield {"app": app, "launcher": launcher, "reg": reg, "lab": lab_files, "out": out_dir}
    finally:
        atlas._cleanup_all_spawned_procs()


def _run(ctx: Dict[str, Any], title: str, fn) -> None:
    reg = ctx["reg"]
    before = len(reg.checks_failed)
    reg.run_section(title, fn)
    if len(reg.checks_failed) > before:
        name, detail = reg.checks_failed[-1]
        pytest.fail(f"{name}\n{detail}", pytrace=False)


def test_persona_1_novice(atlas_context: Dict[str, Any]) -> None:
    c = atlas_context
    _run(c, "Persona 1", lambda: atlas.run_persona_1_novice(c["app"], c["reg"], c["lab"]))


def test_persona_2_intermediate(atlas_context: Dict[str, Any]) -> None:
    c = atlas_context
    _run(c, "Persona 2", lambda: atlas.run_persona_2_intermediate(c["app"], c["reg"], c["out"]))


def test_persona_3_expert(atlas_context: Dict[str, Any]) -> None:
    c = atlas_context
    _run(
        c,
        "Persona 3",
        lambda: atlas.run_persona_3_expert(c["app"], c["launcher"], c["reg"], c["lab"], c["out"]),
    )


def test_persona_4_ai_agent_autonomy(atlas_context: Dict[str, Any]) -> None:
    c = atlas_context
    _run(c, "Persona 4", lambda: atlas.run_persona_4_ai_agent_autonomy(c["app"], c["launcher"], c["reg"]))
    assert len(c["reg"].primary_ids) == 30, c["reg"].primary_ids
