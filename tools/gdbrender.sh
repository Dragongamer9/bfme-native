#!/usr/bin/env bash
# Like gdbrun.sh but rendering (SDL offscreen + Mesa lavapipe): backtrace of the first crash.
#   tools/gdbrender.sh <data dir> <script> [map]
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DATA="$(cd "$1" && pwd)"
SCRIPT="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
MAP="${3:-Maps/BfmeTest.map}"
B="$ROOT/engine/GeneralsX/build/lin"
ICD="${LAVAPIPE_ICD:-/root/deps/mesa-install/share/vulkan/icd.d/lvp_icd.x86_64.json}"
export CNC_GENERALS_ZH_PATH="$DATA" CNC_GENERALS_PATH="$DATA" BFME_TEST="$SCRIPT"
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy VK_ICD_FILENAMES="$ICD" VK_DRIVER_FILES="$ICD"
export LD_LIBRARY_PATH="$B/_deps/dxvk-src/lib:$B/_deps/sdl3-build:$B/_deps/sdl3_image-build:${LD_LIBRARY_PATH:-}"
cd "$DATA"
timeout ${BFME_TIMEOUT:-1200} gdb -q -batch -ex "handle SIGPIPE nostop" -ex run -ex "bt 25" --args "$B/GeneralsMD/GeneralsXZH" -win -file "$MAP" -noshellmap -quickstart 2>&1 | grep -aE "${BFME_GREP:-^#|signal|SIGSEGV|\[BFMETEST\]|\[bfme\]|err:}" | head -${BFME_HEAD:-60}
