#!/usr/bin/env python3
"""Dump a null-terminated char* name table from game.dat that contains a given string."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from sagepe import PE

def is_name(pe, p):
    if p == 0 or not pe.is_data(p): return False
    s = pe.cstr(p, 80)
    if isinstance(s, bytes): s = s.decode('latin1')
    return bool(s) and all(c.isupper() or c.isdigit() or c in '_' for c in s)

def dump(pe, ref, stride=4):
    a = ref
    while is_name(pe, pe.u32(a - stride)): a -= stride
    out = []
    while is_name(pe, pe.u32(a)):
        c = pe.cstr(pe.u32(a), 80); out.append(c.decode('latin1') if isinstance(c, bytes) else c); a += stride
    return out

if __name__ == '__main__':
    pe = PE(sys.argv[1]); idx = pe.ptr_index()
    stride = int(sys.argv[3]) if len(sys.argv) > 3 else 4
    for r in idx.get(pe.string_va(sys.argv[2]), []):
        t = dump(pe, r, stride)
        print(hex(r), len(t), ' '.join(t))
