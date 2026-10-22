#!/usr/bin/env bash
# Start a short headless skirmish (Men vs Mordor) on every map: a load and crash check.
#   tools/mapsweep.sh <data dir> [frames] [regex on map name]
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DATA="$(cd "$1" && pwd)"; FRAMES="${2:-300}"; PAT="${3:-.}"
EXE="$ROOT/engine/GeneralsX/build/lin/GeneralsMD/GeneralsXZH"; B="$(dirname "$(dirname "$EXE")")"
export CNC_GENERALS_ZH_PATH="$DATA" CNC_GENERALS_PATH="$DATA" SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
export LD_LIBRARY_PATH="$B/_deps/dxvk-src/lib:$B/_deps/sdl3-build:$B/_deps/sdl3_image-build:${LD_LIBRARY_PATH:-}"
T="$(mktemp -d)"; echo "logs: $T"
printf '%d players\n%d quit\n' "$((FRAMES-2))" "$FRAMES" > "$T/s.txt"
cd "$DATA"
python3 "$ROOT/tools/bigls.py" "$DATA" ".map" | grep -oiE "maps\\\\map (mp|wor) [^\\\\]*\\\\" | sed 's/^maps\\//; s/\\$//' | sort -u | grep -E "$PAT" | while read -r m; do
  BFME_TEST="$T/s.txt" timeout ${BFME_TIMEOUT:-300} "$EXE" -win -headless -skirmish "Maps/$m.map" Men Mordor -noshellmap -quickstart > "$T/$m.log" 2>&1
  rc=$?
  st=$(grep -ac "quit at frame" "$T/$m.log")
  pl=$(grep -a "BFMETEST\] player .*name='Player_1'" "$T/$m.log" | grep -o "structures=[0-9]* units=[0-9]*")
  echo "$([ "$st" -gt 0 ] && echo ok || echo FAIL) rc=$rc  $m  $pl"
done
