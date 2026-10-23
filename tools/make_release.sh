#!/usr/bin/env bash
# Package a prebuilt Linux release: the engine, the libraries it was built against, and play.sh.
#   tools/make_release.sh [version]      -> dist/bfme-native-<version>-linux-x86_64.tar.gz
#
# Layout (play.sh picks the right libraries at launch, see run_release in play.sh):
#   bin/GeneralsXZH     the engine (stripped)
#   lib/                its own libraries (SDL3, DXVK, GameSpy) - always used
#   lib/sys/            every other library it links; used only when the system lacks that library
#   lib/cxx/            libstdc++ / libgcc_s; used when the system's are older than the build's
#   lib/glibc/          glibc and its loader; used when the system's glibc is older than the build's
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VER="${1:-$(date +%Y.%m.%d)}"
B="$ROOT/engine/GeneralsX/build/lin"
NAME="bfme-native-$VER-linux-x86_64"
OUT="$ROOT/dist/$NAME"
rm -rf "$OUT"; mkdir -p "$OUT/bin" "$OUT/lib/sys" "$OUT/lib/cxx" "$OUT/lib/glibc" "$OUT/tools"

strip -o "$OUT/bin/GeneralsXZH" "$B/GeneralsMD/GeneralsXZH"
own=("$B"/_deps/sdl3-build/libSDL3.so.0 "$B"/_deps/sdl3_image-build/libSDL3_image.so.0 "$B"/libgamespy.so)
for f in "$B"/_deps/dxvk-src/lib/*.so.0; do own+=("$f"); done
for f in "${own[@]}"; do strip -o "$OUT/lib/$(basename "$f")" "$f"; done

# no build-machine paths: the engine finds its libraries next to it ($ORIGIN), and every library is
# also there under its plain name (the engine dlopens libdxvk_d3d8.so)
patchelf --set-rpath '$ORIGIN/../lib' "$OUT/bin/GeneralsXZH"
for f in "$OUT"/lib/*.so*; do
  patchelf --set-rpath '$ORIGIN' "$f"
  case "$f" in *.so.0) ln -sfn "$(basename "$f")" "${f%.0}" ;; esac
done

export LD_LIBRARY_PATH="$B/_deps/dxvk-src/lib:$B/_deps/sdl3-build:$B/_deps/sdl3_image-build:$B"
glibc_re='^(ld-linux-x86-64\.so\.2|libc\.so\.6|libm\.so\.6|libresolv\.so\.2|libpthread\.so\.0|libdl\.so\.2|librt\.so\.1|libutil\.so\.1)$'
# the dependency closure of the engine and its own libraries, plus what DXVK loads at run time
{ ldd "$B/GeneralsMD/GeneralsXZH"; for f in "${own[@]}"; do ldd "$f"; done; } | awk '/=> \//{print $1" "$3} /^\t\/.*ld-linux/{print "ld-linux-x86-64.so.2 "$1}' | sort -u |
while read -r so path; do
  case "$path" in "$B"/*) continue ;; esac
  if [[ "$so" =~ $glibc_re ]]; then cp -L "$path" "$OUT/lib/glibc/$so"
  elif [[ "$so" == libstdc++.so.6 || "$so" == libgcc_s.so.1 ]]; then cp -L "$path" "$OUT/lib/cxx/$so"
  else cp -L "$path" "$OUT/lib/sys/$so"; fi
done
[ -f "$OUT/lib/glibc/ld-linux-x86-64.so.2" ] || cp -L /lib64/ld-linux-x86-64.so.2 "$OUT/lib/glibc/"
cp -L /lib/x86_64-linux-gnu/libvulkan.so.1 "$OUT/lib/sys/" 2>/dev/null || true	# DXVK dlopens it
strip --strip-unneeded "$OUT"/lib/sys/*.so* "$OUT"/lib/cxx/*.so* 2>/dev/null || true
# what the build needs from glibc / libstdc++ (the launcher compares against the system)
objdump -T "$OUT/bin/GeneralsXZH" "$OUT"/lib/*.so* | grep -oE 'GLIBC_2\.[0-9]+' | sort -uV | tail -1 > "$OUT/lib/glibc/REQUIRED"
objdump -T "$OUT/bin/GeneralsXZH" "$OUT"/lib/*.so* | grep -oE 'GLIBCXX_3\.4\.[0-9]+' | sort -uV | tail -1 > "$OUT/lib/cxx/REQUIRED"

cp "$ROOT/play.sh" "$OUT/"
cp "$ROOT/tools/bigls.py" "$OUT/tools/"
cp "$ROOT/README.md" "$OUT/README.md"
cp "$ROOT/PROGRESS.md" "$OUT/"
[ -f "$ROOT/engine/GeneralsX/LICENSE.md" ] && cp "$ROOT/engine/GeneralsX/LICENSE.md" "$OUT/LICENSE.md"
tar -C "$ROOT/dist" -czf "$ROOT/dist/$NAME.tar.gz" "$NAME"
du -sh "$OUT" "$ROOT/dist/$NAME.tar.gz"
echo "glibc needed: $(cat "$OUT/lib/glibc/REQUIRED"), libstdc++ needed: $(cat "$OUT/lib/cxx/REQUIRED")"
