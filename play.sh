#!/usr/bin/env bash
# Play RotWK + Edain + Völker natively.
#
#   ./play.sh build                                 one-time: compile the engine (cmake + ninja)
#   ./play.sh setup [<RotWK folder> <BFME2 folder>] one-time: link the game files into a data folder
#                                                   (no folders: finds a Beacon install)
#   ./play.sh [options]                             start a skirmish: you vs one computer player
#       --map NAME      map folder name, e.g. "map mp umbar" (default: Edain camps) - see ./play.sh maps
#       --you SIDE      your faction (default Men)    - see ./play.sh factions
#       --ai SIDE       the computer's faction (default Mordor)
#       --data DIR      data folder (default ~/.local/share/bfme-native/data or $BFME_DATA)
#       --size WxH      window size (default 1280x720)    --fullscreen   full screen
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
# a prebuilt release ships the engine in bin/ and its libraries in lib/ (tools/make_release.sh)
RELEASE=; [ -x "$ROOT/bin/GeneralsXZH" ] && { RELEASE=1; EXE="$ROOT/bin/GeneralsXZH"; }

# release: use the system's libraries where it has them (GPU drivers must see their own), the bundled
# ones for the rest; the bundled glibc / libstdc++ only when the system's are older than the build's
run_release() {
  local L="$ROOT/lib" RT="${XDG_CACHE_HOME:-$HOME/.cache}/bfme-native/lib" LDC
  LDC="$(command -v ldconfig || echo /sbin/ldconfig)"
  local cache; cache="$("$LDC" -p 2>/dev/null)"
  rm -rf "$RT"; mkdir -p "$RT"
  for f in "$L"/sys/*; do
    local so; so="$(basename "$f")"
    grep -qE "^[[:space:]]*${so//+/\\+} \\(.*x86-64" <<<"$cache" || ln -s "$f" "$RT/$so"
  done
  local LP="$L:$RT" need_cxx syscxx need_glibc sys_glibc
  need_cxx="$(cat "$L/cxx/REQUIRED")"
  syscxx="$(awk '/libstdc\+\+\.so\.6 .*x86-64/{print $NF; exit}' <<<"$cache")"
  if [ -z "$syscxx" ] || ! grep -aq "$need_cxx" "$syscxx"; then LP="$L:$L/cxx:$RT"; fi
  need_glibc="$(sed 's/GLIBC_//' "$L/glibc/REQUIRED")"
  sys_glibc="$(ldd --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+$')"
  if [ -n "$sys_glibc" ] && [ "$(printf '%s\n' "$sys_glibc" "$need_glibc" | sort -V | head -1)" = "$need_glibc" ]; then
    export LD_LIBRARY_PATH="$LP${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    exec "$EXE" "$@"
  fi
  echo "play.sh: system glibc ${sys_glibc:-?} is older than $need_glibc - using the bundled one" >&2
  exec "$L/glibc/ld-linux-x86-64.so.2" --library-path "$LP:$L/glibc" "$EXE" "$@"
}

setup() {
  local rotwk="$(cd "$1" && pwd)" bfme2="$(cd "$2" && pwd)" f n lc
  # (a [g] in a name makes bash match it as a pattern, so nocaseglob applies)
  # installs differ in capitals (INI.big, W3D.big...): match names case-insensitively and link
  # them lowercase, so archives keep the order the game expects (mods' '_' names first)
  shopt -s nullglob nocaseglob
  [ -n "$(echo "$rotwk"/ini.bi[g])" ] || die "$rotwk has no ini.big - is it the RotWK folder?"
  [ -n "$(echo "$rotwk"/*edain*.big)" ] || echo "warning: no Edain .big files in $rotwk - the game starts as plain RotWK"
  mkdir -p "$DATA/Maps" "$DATA/lang"
  find "$DATA" -maxdepth 2 -type l -delete
  lc() { printf '%s' "$1" | tr '[:upper:]' '[:lower:]'; }
  for f in "$rotwk"/*.big; do ln -sfn "$f" "$DATA/$(lc "$(basename "$f")")"; done
  for f in "$rotwk"/apt/*.big; do ln -sfn "$f" "$DATA/aptui_$(lc "$(basename "$f")")"; done
  for f in "$rotwk"/lang/*.big; do ln -sfn "$f" "$DATA/lang/$(lc "$(basename "$f")")"; done
  for f in "$rotwk"/dat[a]; do [ -d "$f" ] && ln -sfn "$f" "$DATA/data"; done	# loose files: cursors, movies
  # BFME2's art, sounds, voices and music (RotWK's own audio.big only holds its new sounds)
  for n in terrain textures0 textures1 textures2 textures3 textures4 w3d audio music speech; do
    for f in "$bfme2"/$n.bi[g]; do ln -sfn "$f" "$DATA/zz_bfme2_$n.big"; done
  done
  for f in "$bfme2"/lang/*/*.big "$bfme2"/lang/*.big; do ln -sfn "$f" "$DATA/zz_bfme2_lang_$(lc "$(basename "$f")")"; done
  [ -n "$(echo "$bfme2"/audio.bi[g])" ] || echo "warning: no audio.big in $bfme2 - BFME2's sounds will be missing"
  for f in "$bfme2"/apt/playerstatus.bi[g] "$bfme2"/apt/scorescreen.bi[g] "$bfme2"/apt/aptui_playerstatus.bi[g] "$bfme2"/apt/aptui_scorescreen.bi[g]; do
    n="$(lc "$(basename "$f")")"; ln -sfn "$f" "$DATA/zz_bfme2_aptui_${n#aptui_}"
  done
  shopt -u nullglob nocaseglob
  echo "data folder ready: $DATA ($(ls "$DATA"/*.big | wc -l) archives)"
}

case "${1:-}" in
  build)
    [ -n "$RELEASE" ] && { echo "this is a prebuilt release: nothing to build"; exit 0; }
    # the engine is the bfme-rotwk branch of this repository: fetch it next to the toolkit once
    if [ ! -f "$ROOT/engine/GeneralsX/CMakeLists.txt" ]; then
      for t in git cmake ninja; do command -v $t >/dev/null || die "$t is not installed (needed to build)"; done
      git clone --depth 1 -b bfme-rotwk https://github.com/Dragongamer9/bfme-native.git "$ROOT/engine/GeneralsX" ||
        die "could not download the engine (branch bfme-rotwk)"
    fi
    for t in cmake ninja; do command -v $t >/dev/null || die "$t is not installed (needed to build)"; done
    command -v c++ >/dev/null || command -v g++ >/dev/null || command -v clang++ >/dev/null || die "no C++ compiler installed"
    cmake -S "$ROOT/engine/GeneralsX" -B "$ROOT/engine/GeneralsX/build/lin" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DRTS_BUILD_GENERALS=OFF -DRTS_BUILD_GENERALS_TOOLS=OFF -DRTS_BUILD_ZEROHOUR_TOOLS=OFF \
      -DRTS_BUILD_CORE_TOOLS=OFF -DRTS_BUILD_CORE_EXTRAS=OFF && ninja -C "$ROOT/engine/GeneralsX/build/lin" GeneralsXZH
    exit $? ;;
  setup)
    if [ $# -eq 1 ]; then	# no folders given: look where game launchers install (Beacon first)
      r=; b=
      for d in "${XDG_DATA_HOME:-$HOME/.local/share}/beacon/games" "$HOME/Games" "$HOME/.local/share/Steam/steamapps/common"; do
        [ -d "$d" ] || continue
        [ -z "$r" ] && r="$(find "$d" -maxdepth 4 -iname ini.big -path '*[Ww]itch*' -printf '%h\n' 2>/dev/null | head -1)"
        [ -z "$r" ] && r="$(find "$d" -maxdepth 4 -iname ini.big -ipath '*rotwk*' -printf '%h\n' 2>/dev/null | head -1)"
        [ -z "$b" ] && b="$(find "$d" -maxdepth 4 -iname ini.big -printf '%h\n' 2>/dev/null | grep -iv 'witch\|rotwk' | grep -i 'bfme2\|middle-earth ii\|middle earth ii\|bfme 2' | head -1)"
      done
      [ -n "$r" ] && [ -n "$b" ] || die "could not find the games; run ./play.sh setup <RotWK folder> <BFME2 folder>"
      echo "found RotWK: $r"; echo "found BFME2: $b"
      set -- setup "$r" "$b"
    fi
    [ $# -eq 3 ] || die "usage: ./play.sh setup [<RotWK folder> <BFME2 folder>]"; setup "$2" "$3"; exit 0 ;;
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

MAP="map mp umbar"; YOU=Men; AI=Mordor; SIZE=1280x720; MODE=-win
while [ $# -gt 0 ]; do
  case "$1" in
    --map) MAP="$2"; shift 2 ;;
    --you) YOU="$2"; shift 2 ;;
    --ai) AI="$2"; shift 2 ;;
    --data) DATA="$2"; shift 2 ;;
    --size) SIZE="$2"; shift 2 ;;
    --fullscreen) MODE=-fullscreen; shift ;;
    *) die "unknown option $1 (see the top of play.sh)" ;;
  esac
done
[ -x "$EXE" ] || die "engine not built: run ./play.sh build (or download a prebuilt release)"
[ -f "$DATA/ini.big" ] || die "no game data in $DATA - run ./play.sh setup first"
MAP="${MAP%.map}"; MAP="${MAP#Maps/}"
B="$ROOT/engine/GeneralsX/build/lin"
export CNC_GENERALS_ZH_PATH="$DATA" CNC_GENERALS_PATH="$DATA"
export LD_LIBRARY_PATH="$B/_deps/dxvk-src/lib:$B/_deps/sdl3-build:$B/_deps/sdl3_image-build:${LD_LIBRARY_PATH:-}"
cd "$DATA"
[ -n "$RELEASE" ] && run_release $MODE -xres "${SIZE%x*}" -yres "${SIZE#*x}" -skirmish "Maps/$MAP.map" "$YOU" "$AI" -noshellmap -quickstart
exec "$EXE" $MODE -xres "${SIZE%x*}" -yres "${SIZE#*x}" -skirmish "Maps/$MAP.map" "$YOU" "$AI" -noshellmap -quickstart
