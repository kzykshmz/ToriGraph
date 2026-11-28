#!/usr/bin/env bash
# Linux/macOS counterpart of scripts/dev.ps1 for review / debugging sessions.
#
#   scripts/dev.sh deps                 # apt: Qt5, GSL, muParser, Xvfb, ccache (+ pip test deps)
#   scripts/dev.sh asan                 # configure + build build-asan/ (GCC ASan+UBSan, Ninja, ccache)
#   scripts/dev.sh suites [81,82,...]   # portable C++ regression suites under ASan+UBSan
#   scripts/dev.sh corpus               # generate the adversarial test-data corpus (build-fuzz/corpus)
#   scripts/dev.sh fuzz [args...]       # parallel JSON-RPC fuzzing of the ASan build (scripts/fuzz/rpc_fuzz.py)
#   scripts/dev.sh py-test              # offline + live Python SDK tests against the ASan build
#   scripts/dev.sh lint                 # version / glossary / static hygiene / ruff
#
# Environment: BUILD_DIR (default build-asan), JOBS (default nproc).
# TORIGRAPH_NO_OLE_REGISTER=1 is always set (AGENTS.md safety rule).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-asan}"
JOBS="${JOBS:-$(nproc)}"
export TORIGRAPH_NO_OLE_REGISTER=1
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
export QT_OPENGL=software
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:halt_on_error=1:detect_stack_use_after_return=0:allocator_may_return_null=1}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}"

SAN_FLAGS="-O1 -g1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer"

cmd="${1:-help}"
shift || true

case "$cmd" in
  deps)
    sudo apt-get update
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -y qtbase5-dev qtbase5-dev-tools libqt5svg5-dev \
      qttools5-dev qttools5-dev-tools libqt5opengl5-dev libgsl-dev libmuparser-dev zlib1g-dev \
      libgl-dev libglu1-mesa-dev ccache ninja-build cmake xvfb
    git -C "$ROOT" submodule update --init 3rdparty/qwt5-qt5 3rdparty/liborigin
    python3 -m pip install -q pytest pytest-xdist hypothesis numpy ruff
    ;;
  asan)
    cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DTORIGRAPH_USE_CCACHE=OFF \
      -DCMAKE_CXX_FLAGS="$SAN_FLAGS" -DCMAKE_C_FLAGS="$SAN_FLAGS" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
      -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address,undefined" \
      -DSEARCH_FOR_UPDATES=OFF -DORIGIN_IMPORT=ON -DTORIGRAPH_BUILD_PORTABLE_SUITES=ON
    ninja -C "$BUILD_DIR" -j "$JOBS" scidavis scidavis_suite_test "$@"
    ;;
  suites)
    sel="${1:-}"
    exe="$BUILD_DIR/scidavis/scidavis_suite_test"
    if [[ -n "$sel" ]]; then
      "$exe" "--suite=$sel"
    else
      # One process per suite in parallel: isolates state, and a crash only fails that suite.
      mapfile -t ids < <("$exe" --list-suites | sed -n 's/^Suite \([0-9]*\):.*/\1/p')
      logdir="$ROOT/build-fuzz/suites"
      mkdir -p "$logdir"
      printf '%s\n' "${ids[@]}" | xargs -P "$JOBS" -I{} sh -c \
        "'$exe' --suite={} > '$logdir/suite_{}.log' 2>&1 && echo 'PASS {}' || echo 'FAIL {}'" | sort -k2 -n
    fi
    ;;
  corpus)
    python3 "$ROOT/scripts/fuzz/gen_corpus.py" "${1:-$ROOT/build-fuzz/corpus}"
    ;;
  fuzz)
    [[ -d "$ROOT/build-fuzz/corpus" ]] || python3 "$ROOT/scripts/fuzz/gen_corpus.py"
    python3 "$ROOT/scripts/fuzz/rpc_fuzz.py" --exe "$BUILD_DIR/scidavis/ToriGraph" "$@"
    ;;
  py-test)
    export TORIGRAPH_EXE="$BUILD_DIR/scidavis/ToriGraph"
    cd "$ROOT/python" && python3 -m pytest -q -n "$JOBS" -p no:cacheprovider tests "$@"
    ;;
  lint)
    python3 "$ROOT/scripts/check_version.py"
    python3 "$ROOT/scripts/check_ja_glossary.py"
    python3 "$ROOT/scripts/check_static_hygiene.py"
    (cd "$ROOT" && python3 -m ruff check --config python/pyproject.toml python scripts/check_version.py scripts/check_ja_glossary.py scripts/check_static_hygiene.py scripts/fuzz)
    ;;
  *)
    sed -n '2,14p' "$0"
    ;;
esac
