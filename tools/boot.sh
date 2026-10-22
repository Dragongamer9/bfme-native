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
# without a test script, stop right after start-up (the engine now keeps running without music)
if [[ -z "${BFME_TEST:-}" ]]; then
  export BFME_TEST="$DATA/.boot_quit.txt"
  echo "30 quit" > "$BFME_TEST"
fi
ARGS=(-win)
EXTRA=(${BFME_ARGS:-})
LVP_ICD="${LAVAPIPE_ICD:-/root/deps/mesa-install/share/vulkan/icd.d/lvp_icd.x86_64.json}"
if [[ -n "${BFME_RENDER:-}" ]]; then
  # render without a desktop: SDL offscreen window + Mesa lavapipe (software Vulkan, has what DXVK needs)
  export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy VK_ICD_FILENAMES="$LVP_ICD" VK_DRIVER_FILES="$LVP_ICD"
elif [[ -z "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]]; then
  # no desktop: the engine's -headless mode skips rendering (software Vulkan lacks what DXVK needs)
  ARGS=(-win -headless)
  export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
  [[ -f "$ICD" ]] && export VK_ICD_FILENAMES="$ICD" VK_DRIVER_FILES="$ICD"
fi
# runtime libraries built alongside the engine (DXVK native, SDL3)
BUILD="$(dirname "$(dirname "$EXE")")"
export LD_LIBRARY_PATH="$BUILD/_deps/dxvk-src/lib:$BUILD/_deps/sdl3-build:$BUILD/_deps/sdl3_image-build:${LD_LIBRARY_PATH:-}"
cd "$DATA"
timeout "$SECS" "$EXE" "${ARGS[@]}" "${EXTRA[@]}" > boot.log 2>&1
rc=$?
echo "exit code: $rc"
echo "INI files loaded: $(grep -c "\[INI\] load('.*') END" boot.log)"
echo "last subsystem:   $(grep -oE "initSubsystem\('[^']+'\) START" boot.log | tail -1)"
grep -E "\[INI-PP\]|ERROR in load|^  > |FATAL|Segmentation|Assertion" boot.log | head -12
echo "bad fields (parse errors, skipped): $(grep -c '^\[INI\] bad field' boot.log)"
grep '^\[INI\] bad field' boot.log | sed -E "s/ at .*//" | sort | uniq -c | sort -rn | head -15
if [[ -s ini_report.tsv ]]; then
  echo "unimplemented modules hit: $(grep -c $'\tmodule\t' ini_report.tsv)  (instances: $(awk -F'\t' '$2=="module"{s+=$1}END{print s+0}' ini_report.tsv))"
  echo "skipped INI items (top 15 of $(wc -l < ini_report.tsv)):"
  sort -rn ini_report.tsv | head -15
fi
