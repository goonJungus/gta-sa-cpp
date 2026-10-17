#!/usr/bin/env bash
# Linux: run the MinGW build under Wine. Usage: scripts/run_wine.sh /path/to/GTA_SA [engine args]
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
game="$1"; shift
export WINEPREFIX="${WINEPREFIX:-$HOME/.wine-gta}"
exec wine "$here/build/mingw/gta_standalone.exe" --game "Z:${game//\//\\}" "$@"
