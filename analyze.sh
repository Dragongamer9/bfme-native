#!/usr/bin/env bash
# Full analysis pass over an installed game (+ mods).
#
#   ./analyze.sh <RotWK dir> [<BFME2 dir>] [<Edain dir>] [<submod dir>] ...
#
# Directories are given in load order (base game first, mods last; later wins).
# The first directory must contain the game executable (game.dat or lotrbfme2ep1.exe).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$ROOT/data"
GAME="$1"
mkdir -p "$OUT"

# pick the real engine binary: the largest 32-bit PE among the usual names
EXE=""
best=0
for c in "$GAME"/game.dat "$GAME"/lotrbfme2ep1.exe "$GAME"/lotrbfme2.exe "$GAME"/*.exe "$GAME"/*.dat; do
  [[ -f "$c" ]] || continue
  [[ "$(head -c2 "$c")" == "MZ" ]] || continue
  sz=$(stat -c %s "$c")
  (( sz > best )) && { best=$sz; EXE="$c"; }
done
[[ -n "$EXE" ]] || { echo "no game executable found in $GAME" >&2; exit 1; }
echo "engine binary: $EXE ($best bytes)"
sha256sum "$EXE" | tee "$OUT/exe.sha256"

python3 "$ROOT/tools/scan_tables.py"  "$EXE" --symbols "$OUT/generals_symbols.json" -o "$OUT"
python3 "$ROOT/tools/scan_modules.py" "$EXE" --symbols "$OUT/generals_symbols.json" -o "$OUT" || \
  echo "module registry scan failed; tables still usable" >&2
python3 "$ROOT/tools/merge_labels.py" "$OUT"

# overlay every .big, lowest priority first: SAGE lets the alphabetically FIRST archive win
# (why mods prefix names with underscores), so extract in reverse order and let later files overwrite
DATA="$ROOT/work/data"
rm -rf "$DATA"; mkdir -p "$DATA"
for d in "$@"; do
  find "$d" -maxdepth 2 -iname '*.big' -print0 | sort -rz | while IFS= read -r -d '' b; do
    python3 "$ROOT/tools/bigtool.py" extract "$b" "$DATA" --grep "data\\ini" || true
  done
done
python3 "$ROOT/tools/ini_usage.py" "$DATA" --symbols "$OUT/generals_symbols.json" \
  --modules "$OUT/modules.json" -o "$OUT/ini_usage.json"

echo
echo "Load data/labels.csv into Ghidra with ghidra_scripts/ApplyBfmeLabels.java"
