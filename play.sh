#!/usr/bin/env bash
# Play RotWK + Edain + Völker natively.
#
#   ./play.sh build                                 one-time: compile the engine (cmake + ninja)
#   ./play.sh setup <RotWK folder> <BFME2 folder>   one-time: link the game files into a data folder
#   ./play.sh [options]                             start a skirmish: you vs one computer player
#       --map NAME      map folder name, e.g. "map mp umbar" (default: Edain camps) - see ./play.sh maps
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
  # BFME2's art, sounds, voices and music (RotWK's own audio.big only holds its new sounds)
  for n in terrain textures0 textures1 textures2 textures3 textures4 w3d audio music speech; do
    [ -f "$bfme2/$n.big" ] && ln -sfn "$bfme2/$n.big" "$DATA/zz_bfme2_$n.big"
  done
  for f in "$bfme2"/lang/*/*.big "$bfme2"/lang/*.big; do
    [ -f "$f" ] && ln -sfn "$f" "$DATA/zz_bfme2_lang_$(basename "$f")"
  done
  [ -f "$bfme2/audio.big" ] || echo "warning: no audio.big in $bfme2 - BFME2's sounds will be missing"
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
  factions)	# every playable side in the loaded data (Völker adds 18 to Edain's 9)
    python3 - "$DATA" <<'PY'
import sys, struct, glob, os, re
# read data\ini\playertemplate.ini from the highest-priority archive
best = None
for f in sorted(glob.glob(os.path.join(sys.argv[1], '*.big'))):
    with open(f, 'rb') as fh:
        h = fh.read(16)
        if h[:4] not in (b'BIGF', b'BIG4'): continue
        n = struct.unpack('>I', h[8:12])[0]; hs = struct.unpack('>I', h[12:16])[0]; idx = fh.read(hs); p = 0
        for _ in range(n):
            off, sz = struct.unpack('>II', idx[p:p + 8]); e = idx.index(b'\0', p + 8); name = idx[p + 8:e].decode('latin1').lower(); p = e + 1
            if name == 'data\\ini\\playertemplate.ini':
                fh.seek(off); best = fh.read(sz).decode('latin1'); break
    if best: break
side = None
for line in (best or '').splitlines():
    m = re.match(r'\s*Side\s*=\s*(\S+)', line)
    if m: side = m.group(1)
    if re.match(r'\s*PlayableSide\s*=\s*Yes', line, re.I) and side: print(side)
PY
    exit 0 ;;
esac

MAP="map mp umbar"; YOU=Men; AI=Mordor
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
