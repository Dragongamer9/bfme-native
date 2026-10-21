#!/usr/bin/env bash
# Like logictest.sh but under gdb; prints the backtrace of the first crash.
#   tools/gdbrun.sh <data dir> <script>
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DATA="$(cd "$1" && pwd)"
SCRIPT="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
EXE="${BFME_EXE:-$ROOT/engine/GeneralsX/build/lin/GeneralsMD/GeneralsXZH}"
BUILD="$(dirname "$(dirname "$EXE")")"
mkdir -p "$DATA/Maps/BfmeTest"
python3 "$ROOT/tools/mkmap.py" "$DATA/Maps/BfmeTest/BfmeTest.map" ${MKMAP_ARGS:-} >/dev/null
export CNC_GENERALS_ZH_PATH="$DATA" CNC_GENERALS_PATH="$DATA" BFME_TEST="$SCRIPT"
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
export LD_LIBRARY_PATH="$BUILD/_deps/dxvk-src/lib:$BUILD/_deps/sdl3-build:$BUILD/_deps/sdl3_image-build:${LD_LIBRARY_PATH:-}"
cd "$DATA"
timeout 900 gdb -q -batch -ex "handle SIGPIPE nostop" -ex run -ex "bt 30" --args "$EXE" -win -headless -file Maps/BfmeTest.map -noshellmap -quickstart 2>&1 | grep -aE "^#|signal|SIGSEGV|\[BFMETEST\]|\[bfme\]" | head -60
