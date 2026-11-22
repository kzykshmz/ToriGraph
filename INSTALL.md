# ToriGraph - Build and Installation Notes

Most users should install the prebuilt Windows package (see [README.md](README.md#installation-windows)).
This file describes how to build ToriGraph from source. The only supported build system is **CMake**;
the qmake project files and the SciDAVis-era packaging scripts were removed.

## Requirements

- CMake 3.16 or later and a C++17 compiler (MSVC 2022 on Windows)
- Qt 5.15 (Core, Gui, Widgets, Svg, Network, PrintSupport, Xml, OpenGL, LinguistTools)
- GSL, muParser, zlib
- The git submodules `3rdparty/qwt5-qt5` and `3rdparty/liborigin` (`git clone --recursive`)

On Windows the CI uses a conda-forge environment created with micromamba
(`qt-main=5.15 gsl muparser zlib ninja cmake`, netlib BLAS) and links it as `.deps`
in the repository root; see `.github/workflows/main.yml`.

## Configure and build

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=<prefix of Qt/GSL/muParser> \
      -DORIGIN_IMPORT=ON -DBUILD_QWT=ON -DBUILD_QWTPLOT3D=ON
cmake --build build --target scidavis scidavis_ole_test
```

`scidavis_ole_test` (Windows only) is the regression test program. The Python SDK tests are run with
`pytest python/tests` (set `TORIGRAPH_EXE` to the built `ToriGraph.exe`).

## Version numbers

The version is defined once, in the top-level `CMakeLists.txt` (`project(... VERSION x.y.z)` and
`TORIGRAPH_RELEASE_DATE`). `libscidavis/src/version.cpp` is generated from `version.cpp.in`, and the
Windows installer takes the version from the same place. The Python SDK's `__version__`
(`python/torigraph/__init__.py`) must be updated together; CMake and `scripts/check_version.py`
stop the build if they differ.

## Windows installer

```
pwsh scripts/build-windows-installer.ps1
```

builds the portable ZIP and, when Inno Setup 6 is installed, the setup program in `dist/`.

## License

ToriGraph's source code is distributed under the GNU General Public License, version 2 or (at your
option) any later version; the text of version 2 is in `LICENSE`. Binary distributions are linked
with GPL-3.0-or-later libraries and are therefore distributed under GPL-3.0-or-later
(`LICENSE-GPL-3.0.txt`). See `THIRD_PARTY_LICENSES.md` for the bundled components.
