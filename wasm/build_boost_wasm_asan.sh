#!/usr/bin/env bash
# ASan variant of build_boost_wasm.sh, for debugging the memory-corruption
# hypothesis behind the BuildFileSystemCache hang.
set -euo pipefail

source ~/emsdk/emsdk_env.sh > /dev/null 2>&1

# BOOST_SRC: a boost checkout (github.com/boostorg/boost with submodules, `b2 headers` run)
SRC_DIR="${BOOST_SRC:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/boost-src}"
OUT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/boost-wasm-asan"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR/lib" "$OUT_DIR/include"
ln -sf "$SRC_DIR/boost" "$OUT_DIR/include/boost"

# Flags must match wasm/CMakeLists.txt (see build_boost_wasm.sh).
EMXX="em++ -O0 -g -fsanitize=address -std=c++17 -fexceptions -funsigned-char -D_THREAD_SAFE -DCASE_SENSITIVE_FILESYSTEM -DBOOST_DATE_TIME_NO_LIB -DBOOST_FILESYSTEM_SINGLE_THREADED -I$SRC_DIR -DBOOST_ALL_NO_LIB=1"

compile_lib() {
  local name="$1"
  shift
  local objdir="$OUT_DIR/obj-$name"
  mkdir -p "$objdir"
  echo "== building lib$name (asan) =="
  local objs=()
  for src in "$@"; do
    local obj="$objdir/$(basename "${src%.*}").o"
    $EMXX -c "$src" -o "$obj"
    objs+=("$obj")
  done
  emar rcs "$OUT_DIR/lib/libboost_$name.a" "${objs[@]}"
}

compile_lib filesystem \
  "$SRC_DIR"/libs/filesystem/src/*.cpp

compile_lib date_time \
  "$SRC_DIR"/libs/date_time/src/gregorian/greg_month.cpp

compile_lib iostreams \
  "$SRC_DIR"/libs/iostreams/src/zlib.cpp

compile_lib serialization \
  "$SRC_DIR"/libs/serialization/src/*.cpp

echo "== done =="
ls -la "$OUT_DIR/lib"
