# Third-Party Licenses and Attributions

ToriGraph is a derivative work based on **SciDAVis** (which itself originated from **QtiPlot**).
This document summarizes the licenses of upstream code and third-party libraries bundled with or linked by ToriGraph.

---

## Project License Summary

- **Source code (`ToriGraph`, `libscidavis`, `scidavis`, `python/torigraph`)**:
  Licensed under the **GNU General Public License v2.0 or later (`GPL-2.0-or-later`)**. The GPL version 2 text is in [LICENSE](LICENSE).
- **Compiled binary distributions**:
  When distributed as compiled binaries linked with GPL-3.0-or-later / LGPL-3.0 libraries (such as GNU Scientific Library (GSL), Qt 5, or liborigin), the combined work is distributed under the **GNU General Public License v3.0 or later (`GPL-3.0-or-later`)** in accordance with the "or any later version" clause of GPL-2.0-or-later. The GPL version 3 text is in [LICENSE-GPL-3.0.txt](LICENSE-GPL-3.0.txt) (verbatim copy of https://www.gnu.org/licenses/gpl-3.0.txt).
- **Files shipped with the Windows installer / portable ZIP**: `LICENSE`, `LICENSE-GPL-3.0.txt`, `THIRD_PARTY_LICENSES.md` and `README.md` are placed next to `ToriGraph.exe`.

---

## Upstream Projects

| Component | License | Source / Notes |
| --- | --- | --- |
| **SciDAVis** | GPL-2.0-or-later | https://github.com/SciDAVis/scidavis — Base codebase for ToriGraph. All original copyright notices and credits are preserved. |
| **QtiPlot** (0.9-rc2 ancestor) | GPL-2.0-or-later | Historical ancestor of SciDAVis; original contributors are credited in [README.md](README.md). |

---

## Bundled / Submodule Components (`3rdparty/`)

| Component | Path | License | Notes |
| --- | --- | --- | --- |
| **QwtPlot3D** | `3rdparty/qwtplot3d` | zlib-style (QwtPlot3D License) | Copyright (C) 2003–2005 Michael Bieber. See [3rdparty/qwtplot3d/LICENSE](3rdparty/qwtplot3d/LICENSE). |
| **GL2PS** | `3rdparty/qwtplot3d/3rdparty/gl2ps` | LGPL-2.0-or-later / GL2PS License | See [COPYING.LGPL](3rdparty/qwtplot3d/3rdparty/gl2ps/COPYING.LGPL) and [COPYING.GL2PS](3rdparty/qwtplot3d/3rdparty/gl2ps/COPYING.GL2PS). |
| **Qwt 5 (Qt5 port)** | `3rdparty/qwt5-qt5` (submodule) | Qwt License v1.0 (LGPL-2.1 with exceptions) | https://github.com/SciDAVis/qwt5-qt5 |
| **liborigin** | `3rdparty/liborigin` (submodule) | GPL-3.0-or-later | https://github.com/SciDAVis/liborigin — Optional import support for legacy `.opj` files. |
| **minigzip** | `3rdparty/minigzip` | zlib License | Part of zlib utilities. |

---

## External Linked Dependencies

| Library | License | Website |
| --- | --- | --- |
| **Qt 5** | LGPL-3.0 / GPL-3.0 | https://www.qt.io/ |
| **GNU Scientific Library (GSL)** | GPL-3.0-or-later | https://www.gnu.org/software/gsl/ |
| **muParser** | BSD-2-Clause | https://beltoforion.de/en/muparser/ |
| **zlib** | zlib License | https://zlib.net/ |
| **BLAS / CBLAS used by GSL** (netlib reference BLAS or OpenBLAS) | BSD-3-Clause | https://www.netlib.org/blas/ , https://www.openblas.net/ — Intel MKL is intentionally **not** redistributed (the installer build stops if the BLAS in use depends on MKL). |
| **python-pptx** *(optional, for `torigraph` SDK `to_pptx`)* | MIT | https://python-pptx.readthedocs.io/ |

---

## Trademark Notice

- **Origin** and **OriginPro** are registered trademarks of OriginLab Corporation.
- **Microsoft**, **Windows**, and **PowerPoint** are trademarks or registered trademarks of Microsoft Corporation.
- **ISIJ International** is a publication of The Iron and Steel Institute of Japan.

ToriGraph is an independent open-source project and is not affiliated with, endorsed by, or sponsored by OriginLab Corporation, Microsoft Corporation, or The Iron and Steel Institute of Japan.
