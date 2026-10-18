#!/usr/bin/env bash
# Linux: run the MinGW build under Wine. Usage: scripts/run_wine.sh /path/to/GTA_SA [engine args]
set -euo pipefail
# Wayland / Hyprland notes:
#  - Compositors ignore cursor warps from Wine, so mouse look falls back to the cursor's own movement.
#  - If the borderless full-screen window is tiled or mis-sized, run windowed: add `--window 1920x1080`,
#    or float it with a window rule (Hyprland: windowrulev2 = float, class:^(gta_standalone.exe)$).
#  - GTA_NORESIZE=1 stops the engine following window resizes (useful when a compositor keeps resizing it).
here="$(cd "$(dirname "$0")/.." && pwd)"
game="$1"; shift
export WINEPREFIX="${WINEPREFIX:-$HOME/.wine-gta}"
exec wine "$here/build/mingw/gta_standalone.exe" --game "Z:${game//\//\\}" "$@"
