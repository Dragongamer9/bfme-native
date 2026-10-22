#!/usr/bin/env python3
"""Find a RotWK INI field name in game.dat's FieldParse tables: prints the parse function, user data
   and offset of every table entry naming it.  fieldparse.py <FieldName> [game.dat]"""
import struct, sys
name = sys.argv[1].encode() + b'\0'
d = open(sys.argv[2] if len(sys.argv) > 2 else 'work/game.dat', 'rb').read()
pe = struct.unpack_from('<I', d, 0x3c)[0]
ns = struct.unpack_from('<H', d, pe + 6)[0]; opt = struct.unpack_from('<H', d, pe + 20)[0]
base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
secs = []
for i in range(ns):
    o = pe + 24 + opt + 40 * i
    vs, va, rs, ra = struct.unpack_from('<IIII', d, o + 8); secs.append((va, rs, ra))
def off2va(off):
    for va, rs, ra in secs:
        if ra <= off < ra + rs: return base + va + off - ra
def va2off(v):
    for va, rs, ra in secs:
        if base + va <= v < base + va + rs: return v - base - va + ra
i = d.find(b'\0' + name)
while i != -1:
    sva = off2va(i + 1)
    ptr = struct.pack('<I', sva)
    j = d.find(ptr)
    while j != -1:
        fn, ud, off = struct.unpack_from('<III', d, j + 4)
        print('string %#x entry %#x: parse %#x userData %#x offset %#x' % (sva, off2va(j), fn, ud, off))
        j = d.find(ptr, j + 1)
    i = d.find(b'\0' + name, i + 1)
