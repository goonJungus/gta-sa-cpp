#!/usr/bin/env bash
# Linux: cross-compile gta_standalone.exe with MinGW (needs mingw-w64-gcc).
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
cmake -S "$here" -B "$here/build/mingw" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$here/scripts/mingw-toolchain.cmake"
cmake --build "$here/build/mingw" --parallel "$(nproc)"
