#!/usr/bin/env bash
# Compiles the small subset of Boost that rlvm needs (filesystem,
# serialization, iostreams(zlib only), date_time) directly to wasm object
# files with em++, bypassing b2/bootstrap's toolset machinery. Boost's
# compiled libraries have no OS-specific requirements for these components,
# so straight source compilation is simpler than fighting b2 for a cross
# build.
set -euo pipefail

source ~/emsdk/emsdk_env.sh > /dev/null 2>&1

# BOOST_SRC: a boost checkout (github.com/boostorg/boost with submodules, `b2 headers` run)
SRC_DIR="${BOOST_SRC:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/boost-src}"
OUT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/boost-wasm"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR/lib" "$OUT_DIR/include"

# Headers: just point at the aggregated boost-src/boost symlink tree.
ln -sf "$SRC_DIR/boost" "$OUT_DIR/include/boost"

# Must match the language/ABI flags in wasm/CMakeLists.txt: mixing C++20 vs
# C++17, exception-enabled vs disabled, or signed vs unsigned char between
# Boost and rlvm objects produces layout/unwinding mismatches at link time.
# BOOST_FILESYSTEM_SINGLE_THREADED: under C++17 Boost.Filesystem's src/
# otherwise expects std::atomic_ref; the wasm build has no threads anyway,
# and the macro is only consulted in src/, not the public headers rlvm uses.
EMXX="em++ -O2 -std=c++17 -fexceptions -funsigned-char -D_THREAD_SAFE -DCASE_SENSITIVE_FILESYSTEM -DBOOST_DATE_TIME_NO_LIB -DBOOST_FILESYSTEM_SINGLE_THREADED -I$SRC_DIR -DBOOST_ALL_NO_LIB=1"

compile_lib() {
  local name="$1"
  shift
  local objdir="$OUT_DIR/obj-$name"
  mkdir -p "$objdir"
  echo "== building lib$name =="
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

# Boost's own CMakeLists (libs/date_time/CMakeLists.txt) only compiles
# greg_month.cpp -- the rest of date_time is header-only in modern Boost,
# and compiling the other .cpp files causes ODR conflicts with inline
# definitions that superseded them.
compile_lib date_time \
  "$SRC_DIR"/libs/date_time/src/gregorian/greg_month.cpp

# Only the zlib filter is needed (used by rlvm's save-game archive code);
# skip bzip2/lzma/zstd/file_descriptor/mapped_file to avoid pulling in
# libraries with no Emscripten port.
compile_lib iostreams \
  "$SRC_DIR"/libs/iostreams/src/zlib.cpp

compile_lib serialization \
  "$SRC_DIR"/libs/serialization/src/*.cpp

echo "== done =="
ls -la "$OUT_DIR/lib"
