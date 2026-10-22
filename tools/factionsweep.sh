#!/usr/bin/env bash
# Headless skirmish with each playable faction (you) against a computer opponent: a crash and
# start-up check for every side.   tools/factionsweep.sh <data dir> [map] [frames] [factions...]
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DATA="$(cd "$1" && pwd)"; MAP="${2:-Maps/map mp argonath.map}"; FRAMES="${3:-1800}"; shift 3 2>/dev/null || shift $#
FACTIONS=("$@"); [ ${#FACTIONS[@]} -eq 0 ] && FACTIONS=(Men Rohan Elves Imladris Dwarves Isengard Mordor Wild Angmar)
EXE="$ROOT/engine/GeneralsX/build/lin/GeneralsMD/GeneralsXZH"; B="$(dirname "$(dirname "$EXE")")"
export CNC_GENERALS_ZH_PATH="$DATA" CNC_GENERALS_PATH="$DATA" SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
export LD_LIBRARY_PATH="$B/_deps/dxvk-src/lib:$B/_deps/sdl3-build:$B/_deps/sdl3_image-build:${LD_LIBRARY_PATH:-}"
T="$(mktemp -d)"; echo "logs: $T"
export BFME_DEBUG_ABILITY=1 BFME_DEBUG_SKIRMISH_AI=1
printf '1 reveal\n%d state\n%d players\n%d quit\n' "$((FRAMES-5))" "$((FRAMES-4))" "$FRAMES" > "$T/s.txt"
cd "$DATA"
for f in "${FACTIONS[@]}"; do
  ai="${BFME_AI:-Mordor}"; [ "$f" = "$ai" ] && ai=Men
  BFME_TEST="$T/s.txt" timeout ${BFME_TIMEOUT:-900} "$EXE" -win -headless -skirmish "$MAP" "$f" "$ai" -noshellmap -quickstart > "$T/$f.log" 2>&1
  rc=$?
  q=$(grep -ac "quit at frame" "$T/$f.log")
  echo "== $f vs $ai: exit $rc $([ "$q" -gt 0 ] && echo ok || echo FAIL)"
  echo "   AI abilities used: $(grep -ac '\[ability\] AI' "$T/$f.log")  AI trains: $(grep -ac '\[skirmishai\].*trains' "$T/$f.log")  attacks: $(grep -ac '\[skirmishai\].*attacks' "$T/$f.log")"
  grep -aE "\[BFMETEST\] player [0-9]+ name=.player|\[crash\]|FATAL|Segmentation|free build|unknown faction" "$T/$f.log" | cut -c1-160 | head -${BFME_LINES:-8}
done
