#!/usr/bin/env bash
# Play RotWK + Edain + Völker natively.
#
#   ./play.sh build                                 one-time: compile the engine (cmake + ninja)
#   ./play.sh setup <RotWK folder> <BFME2 folder>   one-time: link the game files into a data folder
#   ./play.sh [options]                             start a skirmish: you vs one computer player
#       --map NAME      map folder name, e.g. "map mp argonath" (default) - see ./play.sh maps
#       --you SIDE      your faction (default Men)    - see ./play.sh factions
#       --ai SIDE       the computer's faction (default Mordor)
#       --data DIR      data folder (default ~/.local/share/bfme-native/data or $BFME_DATA)
#   ./play.sh maps | factions
#
# The RotWK folder is the installed Rise of the Witch-king with the Edain and Völker .big files
# copied into it (as their installers do); BFME2 is the base game the expansion also reads art from.
# In game: Esc opens the menu (resume / exit).
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA="${BFME_DATA:-$HOME/.local/share/bfme-native/data}"
EXE="$ROOT/engine/GeneralsX/build/lin/GeneralsMD/GeneralsXZH"

die() { echo "play.sh: $*" >&2; exit 1; }

setup() {
  local rotwk="$(cd "$1" && pwd)" bfme2="$(cd "$2" && pwd)"
  [ -f "$rotwk/ini.big" ] || die "$rotwk has no ini.big - is it the RotWK folder?"
  ls "$rotwk"/*edain* >/dev/null 2>&1 || echo "warning: no Edain .big files in $rotwk"
  mkdir -p "$DATA/Maps"
  # RotWK reads its own archives (mods first: their names start with '_'), then BFME2's art
  for f in "$rotwk"/*.big "$rotwk"/apt/*.big; do [ -e "$f" ] && ln -sfn "$f" "$DATA/$(basename "$f")"; done
  [ -d "$rotwk/lang" ] && ln -sfn "$rotwk/lang" "$DATA/lang"
  for n in terrain textures0 textures1 textures2 textures3 textures4 w3d; do
    [ -f "$bfme2/$n.big" ] && ln -sfn "$bfme2/$n.big" "$DATA/zz_bfme2_$n.big"
  done
  for f in "$bfme2"/apt/aptui_playerstatus.big "$bfme2"/apt/aptui_scorescreen.big; do
    [ -f "$f" ] && ln -sfn "$f" "$DATA/zz_bfme2_$(basename "$f")"
  done
  echo "data folder ready: $DATA ($(ls "$DATA"/*.big | wc -l) archives)"
}

case "${1:-}" in
  build)
    cmake -S "$ROOT/engine/GeneralsX" -B "$ROOT/engine/GeneralsX/build/lin" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DRTS_BUILD_GENERALS=OFF -DRTS_BUILD_GENERALS_TOOLS=OFF -DRTS_BUILD_ZEROHOUR_TOOLS=OFF \
      -DRTS_BUILD_CORE_TOOLS=OFF -DRTS_BUILD_CORE_EXTRAS=OFF && ninja -C "$ROOT/engine/GeneralsX/build/lin" GeneralsXZH
    exit $? ;;
  setup) [ $# -eq 3 ] || die "usage: ./play.sh setup <RotWK folder> <BFME2 folder>"; setup "$2" "$3"; exit 0 ;;
  maps) python3 "$ROOT/tools/bigls.py" "$DATA" ".map" | grep -oiE "maps\\\\map (mp|wor) [^\\\\]*\\\\" | sed 's/^maps\\//; s/\\$//' | sort -u; exit 0 ;;
  factions) echo "Men Rohan Elves Imladris Dwarves Isengard Mordor Wild Angmar"; exit 0 ;;
esac

MAP="map mp argonath"; YOU=Men; AI=Mordor
while [ $# -gt 0 ]; do
  case "$1" in
    --map) MAP="$2"; shift 2 ;;
    --you) YOU="$2"; shift 2 ;;
    --ai) AI="$2"; shift 2 ;;
    --data) DATA="$2"; shift 2 ;;
    *) die "unknown option $1 (see the top of play.sh)" ;;
  esac
done
[ -x "$EXE" ] || die "engine not built: run ./play.sh build"
[ -f "$DATA/ini.big" ] || die "no game data in $DATA - run ./play.sh setup first"
MAP="${MAP%.map}"; MAP="${MAP#Maps/}"
B="$ROOT/engine/GeneralsX/build/lin"
export CNC_GENERALS_ZH_PATH="$DATA" CNC_GENERALS_PATH="$DATA"
export LD_LIBRARY_PATH="$B/_deps/dxvk-src/lib:$B/_deps/sdl3-build:$B/_deps/sdl3_image-build:${LD_LIBRARY_PATH:-}"
cd "$DATA"
exec "$EXE" -win -skirmish "Maps/$MAP.map" "$YOU" "$AI" -noshellmap -quickstart
