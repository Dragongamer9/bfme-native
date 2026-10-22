#!/usr/bin/env python3
"""List .big archive entries matching a substring: bigls.py <dir> <pattern>"""
import struct, sys, glob, os
pat = sys.argv[2].lower()
for f in sorted(glob.glob(os.path.join(sys.argv[1], '*.big'))):
    with open(f, 'rb') as fh:
        h = fh.read(16)
        if h[:4] not in (b'BIGF', b'BIG4'):
            continue
        n = struct.unpack('>I', h[8:12])[0]
        hs = struct.unpack('>I', h[12:16])[0]
        idx = fh.read(hs)
        p = 0
        for i in range(n):
            off, sz = struct.unpack('>II', idx[p:p + 8])
            e = idx.index(b'\0', p + 8)
            name = idx[p + 8:e].decode('latin1')
            p = e + 1
            if pat in name.lower():
                print(os.path.basename(f), name, sz, off)
