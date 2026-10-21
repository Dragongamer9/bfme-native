#!/usr/bin/env python3
"""List the chunks (name, version, size) of a SAGE .map file, decompressing RefPack ("EAR\\0").
   mapinfo.py FILE.map [--tree]"""
import struct, sys

def refpack(src):
    # EA RefPack (QFS) decompression
    p = 0
    flags = src[0]; p = 2
    big = flags & 0x80
    n = 4 if big else 3
    size = int.from_bytes(src[p:p+n], 'big'); p += n
    if flags & 0x01:
        p += n  # compressed size present
    out = bytearray()
    while p < len(src):
        b0 = src[p]
        if b0 < 0x80:
            b1 = src[p+1]; p += 2
            lit = b0 & 3; n = ((b0 >> 2) & 7) + 3; off = ((b0 & 0x60) << 3) + b1 + 1
        elif b0 < 0xC0:
            b1, b2 = src[p+1], src[p+2]; p += 3
            lit = b1 >> 6; n = (b0 & 0x3F) + 4; off = ((b1 & 0x3F) << 8) + b2 + 1
        elif b0 < 0xE0:
            b1, b2, b3 = src[p+1], src[p+2], src[p+3]; p += 4
            lit = b0 & 3; n = ((b0 & 0x0C) << 6) + b3 + 5; off = ((b0 & 0x10) << 12) + (b1 << 8) + b2 + 1
        elif b0 < 0xFC:
            p += 1; lit = ((b0 & 0x1F) << 2) + 4; n = 0; off = 0
        else:
            p += 1; lit = b0 & 3; n = 0; off = 0
        out += src[p:p+lit]; p += lit
        for _ in range(n):
            out.append(out[-off])
        if b0 >= 0xFC:
            break
    return bytes(out[:size])

def load(path):
    d = open(path, 'rb').read()
    if d[:4] == b'EAR\0':
        d = refpack(d[8:])
    return d

def parse(d):
    assert d[:4] == b'CkMp', d[:4]
    n = struct.unpack_from('<i', d, 4)[0]; p = 8
    names = {}
    for _ in range(n):
        l = d[p]; p += 1
        s = d[p:p+l].decode('latin1'); p += l
        i = struct.unpack_from('<I', d, p)[0]; p += 4
        names[i] = s
    return names, p

def chunks(d, p, end, names, depth, tree, out):
    while p + 10 <= end:
        cid, ver, size = struct.unpack_from('<IHi', d, p)
        name = names.get(cid, '?%d' % cid)
        out.append((depth, name, ver, size))
        if tree and name in NESTED:
            chunks(d, p + 10 + NESTED[name](d, p + 10), p + 10 + size, names, depth + 1, tree, out)
        p += 10 + size

NESTED = {'ObjectsList': lambda d, p: 0, 'PlayerScriptsList': lambda d, p: 0, 'ScriptList': lambda d, p: 0}

if __name__ == '__main__':
    d = load(sys.argv[1])
    names, p = parse(d)
    out = []
    chunks(d, p, len(d), names, 0, '--tree' in sys.argv, out)
    seen = {}
    for depth, name, ver, size in out:
        key = (depth, name, ver)
        seen[key] = seen.get(key, 0) + 1
    for (depth, name, ver), c in seen.items():
        print('%s%s v%d%s' % ('  ' * depth, name, ver, ' x%d' % c if c > 1 else ''))
