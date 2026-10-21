#!/bin/sh
# usage: dec.sh out.c spec...
out=$1; shift
GHIDRA_MAXMEM=3G nice /root/deps/ghidra_12.1.3_PUBLIC/support/analyzeHeadless /home/claude/bfme-native/work/ghidra_ro rotwk -process game.dat -noanalysis -readOnly -scriptPath /home/claude/bfme-native/ghidra_scripts -postScript DecompileFns.java "$out" "$@" > "$out.log" 2>&1
grep -c "^// ====" "$out"
