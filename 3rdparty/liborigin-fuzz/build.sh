#!/usr/bin/env bash
# Build the standalone liborigin fuzz harness with ASan + UBSan.
#
#   3rdparty/liborigin-fuzz/build.sh <out-dir> [liborigin-src-dir]
#
# Compiles the liborigin sources (OriginFile.cpp, OriginParser.cpp,
# OriginAnyParser.cpp - same list as 3rdparty/liborigin/CMakeLists.txt) plus
# opj_fuzz_main.cpp directly with g++ (or $CXX). Does not touch any CMake build
# directory. Set JOBS (default 2) to limit parallel compiles.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
out="${1:?usage: build.sh <out-dir> [liborigin-src-dir]}"
src="${2:-$here/../liborigin}"
cxx="${CXX:-g++}"
jobs="${JOBS:-2}"
flags=(-std=c++17 -g -O1 -fno-omit-frame-pointer
       -fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all
       -Wall -Wextra -Wno-unused-parameter)

mkdir -p "$out"
ver=$(sed -n 's/^ *VERSION \([0-9.]*\).*/\1/p' "$src/CMakeLists.txt" | head -1)
IFS=. read -r vmaj vmin vpat <<<"${ver:-3.0.0}"
sed -e "s/\${PROJECT_VERSION_MAJOR}/$vmaj/" -e "s/\${PROJECT_VERSION_MINOR}/$vmin/" \
    -e "s/\${PROJECT_VERSION_PATCH}/$vpat/" -e "s/\${PROJECT_VERSION}/$ver/" \
    "$src/config.h.in" > "$out/config.h"

pids=()
for f in OriginFile OriginParser OriginAnyParser; do
  "$cxx" "${flags[@]}" -I"$out" -I"$src" -c "$src/$f.cpp" -o "$out/$f.o" &
  pids+=($!)
  if (( ${#pids[@]} >= jobs )); then wait "${pids[0]}"; pids=("${pids[@]:1}"); fi
done
for p in "${pids[@]}"; do wait "$p"; done
"$cxx" "${flags[@]}" -I"$out" -I"$src" "$here/opj_fuzz_main.cpp" \
  "$out/OriginFile.o" "$out/OriginParser.o" "$out/OriginAnyParser.o" -o "$out/opj_fuzz"
echo "built $out/opj_fuzz"
