#!/usr/bin/env bash
# Headless gameplay test: build a flat test map, run a BFME_TEST script on it, print results.
#   tools/logictest.sh <data dir> <script> [seconds]
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DATA="$(cd "$1" && pwd)"
SCRIPT="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
SECS="${3:-600}"
mkdir -p "$DATA/Maps/BfmeTest"
python3 "$ROOT/tools/mkmap.py" "$DATA/Maps/BfmeTest/BfmeTest.map" ${MKMAP_ARGS:-} >/dev/null
BFME_TEST="$SCRIPT" BFME_ARGS="-file Maps/BfmeTest.map -noshellmap -quickstart" \
  "$ROOT/tools/boot.sh" "$DATA" "$SECS" | head -3
grep -a "\[BFMETEST\]" "$DATA/boot.log"
grep -aE "FATAL|Segmentation|Assertion|terminate called" "$DATA/boot.log" | head -5
