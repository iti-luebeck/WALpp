#!/usr/bin/env bash
# Builds the WAL++ core as WebAssembly for the documentation playground.
#
# Output: docs/playground/walpp.js and docs/playground/walpp.wasm, which
# `make docs` copies into the site. Requires the Emscripten SDK on PATH
# (https://emscripten.org, or `brew install emscripten`).
#
# The viewer half of the playground is fetched separately, see tools/fetch-surfer.sh.
set -euo pipefail
cd "$(dirname "$0")"

BUILD_DIR="${BUILD_DIR:-build/wasm}"
OUT_DIR="docs/playground"

command -v emcmake > /dev/null || {
  echo "error: emcmake not found. Install the Emscripten SDK and activate it:" >&2
  echo "  git clone https://github.com/emscripten-core/emsdk && ./emsdk/emsdk install latest" >&2
  echo "  ./emsdk/emsdk activate latest && source ./emsdk/emsdk_env.sh" >&2
  exit 1
}

# zlib comes from an Emscripten port, as the bundled GTKWave fstapi includes <zlib.h>
# unconditionally. Build the port first, then point CMake's find_package at the sysroot:
# a host zlib would be the wrong architecture.
echo "==> building the zlib port"
embuilder build zlib
EM_CACHE="$(em-config CACHE)"
ZLIB_INCLUDE_DIR="${EM_CACHE}/sysroot/include"
ZLIB_LIBRARY="${EM_CACHE}/sysroot/lib/wasm32-emscripten/libz.a"
[ -f "$ZLIB_LIBRARY" ] || { echo "error: no zlib port at $ZLIB_LIBRARY" >&2; exit 1; }

echo "==> configuring"
emcmake cmake -S . -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DZLIB_INCLUDE_DIR="$ZLIB_INCLUDE_DIR" \
  -DZLIB_LIBRARY="$ZLIB_LIBRARY"

echo "==> building"
cmake --build "$BUILD_DIR" --target walpp_wasm -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

mkdir -p "$OUT_DIR"
cp "$BUILD_DIR/walpp.js" "$BUILD_DIR/walpp.wasm" "$OUT_DIR/"

echo "==> done"
ls -la "$OUT_DIR/walpp.js" "$OUT_DIR/walpp.wasm"
