"""pytest entry points for the live end-to-end workflows in ``e2e_full_workflows.py``.

Each workflow section becomes one test.  All of them share the session-scoped
``tg_app`` fixture (one ToriGraph process).  ``python tests/e2e_full_workflows.py``
still runs the same sections as a stand-alone script.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any, Callable, Dict

import pytest

import e2e_full_workflows as wf


def _run_checked(section: Callable[..., Any], *args: Any) -> Any:
    runner = wf.E2ERunner()
    result = section(runner, *args)
    if runner.failed:
        details = "\n\n".join(f"--- {name} ---\n{detail}" for name, detail in runner.failed)
        pytest.fail(
            f"{len(runner.failed)} check(s) failed in {section.__name__}:\n{details}",
            pytrace=False,
        )
    assert runner.passed, f"{section.__name__} ran no checks"
    return result


def test_00_sdk_proxy_contract(tg_app, e2e_tmp_dir: Path) -> None:
    _run_checked(wf.run_section_0_sdk_proxy_contract, tg_app, e2e_tmp_dir)


def test_01_tds_deconvolution(tg_app) -> None:
    _run_checked(wf.run_workflow_1_tds_deconvolution, tg_app)


def test_02_tensile_presets_export(tg_app, e2e_tmp_dir: Path, e2e_refs: Dict[str, Any]) -> None:
    e2e_refs.update(_run_checked(wf.run_workflow_2_tensile_presets_export, tg_app, e2e_tmp_dir) or {})


def test_03_nonlinear_and_regression(tg_app) -> None:
    _run_checked(wf.run_workflow_3_nonlinear_and_regression, tg_app)


def test_04_materials_science(tg_app) -> None:
    _run_checked(wf.run_workflow_4_materials_science, tg_app)


def test_05_signal_and_matrix_3d(tg_app, e2e_refs: Dict[str, Any]) -> None:
    e2e_refs.update(_run_checked(wf.run_workflow_5_signal_and_matrix_3d, tg_app) or {})


def test_06_stats_gadgets_digitizer_project_updater(
    tg_app, e2e_tmp_dir: Path, e2e_refs: Dict[str, Any]
) -> None:
    if not e2e_refs:
        pytest.skip("needs the windows created by test_02 and test_05 in the same session")
    _run_checked(
        wf.run_workflow_6_stats_gadgets_curvemath_digitizer_project_updater,
        tg_app,
        e2e_tmp_dir,
        dict(e2e_refs),
    )


def test_08_round2_critic_regressions(tg_app, e2e_tmp_dir: Path) -> None:
    _run_checked(wf.run_workflow_8_round2_critic_regressions, tg_app, e2e_tmp_dir)


def test_09_round3_critic_regressions(tg_app, e2e_tmp_dir: Path) -> None:
    _run_checked(wf.run_workflow_9_round3_critic_regressions, tg_app, e2e_tmp_dir)


@pytest.mark.slow
@pytest.mark.gui
def test_10_round4_unified_regressions(tg_app, e2e_tmp_dir: Path) -> None:
    # Also starts a second, visible (non-headless) ToriGraph window.
    _run_checked(wf.run_workflow_10_round4_unified_regressions, tg_app, e2e_tmp_dir)


def test_11_round5_zero_defect_gate(tg_app, e2e_tmp_dir: Path) -> None:
    _run_checked(wf.run_workflow_11_round5_zero_defect_gate, tg_app, e2e_tmp_dir)


def test_12_phase_b_api_sdk_gate(tg_app, e2e_tmp_dir: Path) -> None:
    _run_checked(wf.run_workflow_12_phase_b_api_sdk_gate, tg_app, e2e_tmp_dir)


@pytest.mark.slow
def test_13_megawave1_sdk_pptx_matsci_gate(tg_app, e2e_tmp_dir: Path) -> None:
    pytest.importorskip("pptx", reason="python-pptx is needed for the PPTX checks")
    _run_checked(wf.run_workflow_13_megawave1_sdk_pptx_matsci_gate, tg_app, e2e_tmp_dir)


def test_07_cli_run_python(torigraph_exe: Path, e2e_tmp_dir: Path) -> None:
    _run_checked(wf.run_workflow_7_cli_run_python, e2e_tmp_dir)


def test_08b_named_pipe_cli(torigraph_exe: Path, e2e_tmp_dir: Path) -> None:
    _run_checked(wf.run_workflow_8b_named_pipe_cli, e2e_tmp_dir)
