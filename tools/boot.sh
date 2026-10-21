#!/usr/bin/env bash
# Boot the native engine against a RotWK data directory and report how far it got.
#   tools/boot.sh <dir with the game's .big files> [seconds]
# Headless containers: uses SDL offscreen video, dummy audio and the SwiftShader Vulkan ICD
# when SWIFTSHADER_ICD is set (or deps/swiftshader is built).
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DATA="$(cd "$1" && pwd)"
SECS="${2:-300}"
EXE="${BFME_EXE:-$ROOT/engine/GeneralsX/build/lin/GeneralsMD/GeneralsXZH}"
ICD="${SWIFTSHADER_ICD:-/root/deps/swiftshader/b/Linux/vk_swiftshader_icd.json}"

export CNC_GENERALS_ZH_PATH="$DATA" CNC_GENERALS_PATH="$DATA"
export BFME_INI_REPORT="$DATA/ini_report.tsv"
if [[ -z "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]]; then
  export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
  [[ -f "$ICD" ]] && export VK_ICD_FILENAMES="$ICD" VK_DRIVER_FILES="$ICD"
fi
cd "$DATA"
timeout "$SECS" "$EXE" -win > boot.log 2>&1
rc=$?
echo "exit code: $rc"
echo "INI files loaded: $(grep -c "\[INI\] load('.*') END" boot.log)"
echo "last subsystem:   $(grep -oE "initSubsystem\('[^']+'\) START" boot.log | tail -1)"
grep -E "\[INI-PP\]|ERROR in load|^  > |FATAL|Segmentation|Assertion" boot.log | head -12
if [[ -s ini_report.tsv ]]; then
  echo "skipped INI items (top 15 of $(wc -l < ini_report.tsv)):"
  sort -rn ini_report.tsv | head -15
fi
