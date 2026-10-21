#!/usr/bin/env bash
# One-time setup on Linux: analysis deps (capstone, pefile) and the GeneralsX engine base.
# Everything is fetched from GitHub with git; no pip/apt network access is needed.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPS="$ROOT/deps"
mkdir -p "$DEPS"

clone() { # url dir [ref]
  [[ -d "$2/.git" ]] && { echo "have $(basename "$2")"; return; }
  git clone --depth 1 ${3:+-b "$3"} "$1" "$2"
}

clone https://github.com/capstone-engine/capstone.git "$DEPS/capstone" 5.0.6
clone https://github.com/erocarrera/pefile.git        "$DEPS/pefile"
clone https://github.com/fbraz3/GeneralsX.git          "$ROOT/engine/GeneralsX"

if [[ ! -f "$DEPS/capstone/bindings/python/capstone/lib/libcapstone.so" ]]; then
  cmake -S "$DEPS/capstone" -B "$DEPS/capstone/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCAPSTONE_ARCHITECTURE_DEFAULT=OFF -DCAPSTONE_X86_SUPPORT=ON \
        -DBUILD_SHARED_LIBS=ON -DCAPSTONE_BUILD_TESTS=OFF
  ninja -C "$DEPS/capstone/build"
  mkdir -p "$DEPS/capstone/bindings/python/capstone/lib"
  cp "$DEPS"/capstone/build/libcapstone.so* "$DEPS/capstone/bindings/python/capstone/lib/"
fi

python3 "$ROOT/tools/generals_symbols.py" "$ROOT/engine/GeneralsX" -o "$ROOT/data/generals_symbols.json"
echo "setup done. Next: ./analyze.sh <RotWK install dir> [<mod dir> ...]"
