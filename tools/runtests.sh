#!/usr/bin/env bash
# Run every headless gameplay test in tests/logic and print each one's last lines.
#   tools/runtests.sh <data dir> [test names...]
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DATA="$1"; shift || true
TESTS=("$@")
if [ ${#TESTS[@]} -eq 0 ]; then
  for f in "$ROOT"/tests/logic/*.txt; do TESTS+=("$(basename "$f" .txt)"); done
fi
fail=0
for t in "${TESTS[@]}"; do
  pkill -x GeneralsXZH 2>/dev/null
  out="$("$ROOT/tools/logictest.sh" "$DATA" "$ROOT/tests/logic/$t.txt" 900 2>&1)"
  if echo "$out" | grep -q "quit at frame"; then status=ok; else status=FAIL; fail=1; fi
  echo "== $t [$status]"
  echo "$out" | grep -E "summary|money frame|construction #|revived|state #|xp #|FATAL|Segmentation" | tail -3 | cut -c1-150
done
exit $fail
