#!/usr/bin/env python3
"""List the objects (template, position, angle, owner/name properties) in a SAGE .map/.bse.
   mapobjects.py FILE [regex on the template name]"""
import struct, sys, os, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mapinfo

def read_dict(c, q, names):
    n = struct.unpack_from('<H', c, q)[0]; q += 2
    props = {}
    for _ in range(n):
        key = struct.unpack_from('<I', c, q)[0]; q += 4
        typ = key & 0xff; name = names.get(key >> 8, '?')
        if typ == 0: v = bool(c[q]); q += 1
        elif typ == 1: v = struct.unpack_from('<i', c, q)[0]; q += 4
        elif typ == 2: v = struct.unpack_from('<f', c, q)[0]; q += 4
        elif typ in (3, 5):
            l = struct.unpack_from('<H', c, q)[0]; q += 2
            if typ == 3: v = c[q:q + l].decode('latin1'); q += l
            else: v = c[q:q + 2 * l].decode('utf-16le'); q += 2 * l
        else:
            raise ValueError('prop type %d' % typ)
        props[name] = v
    return props, q

def objects(path):
    d = mapinfo.load(path)
    names, p = mapinfo.parse(d)
    out = []
    def walk(p, end):
        while p + 10 <= end:
            cid, ver, size = struct.unpack_from('<IHi', d, p)
            nm = names.get(cid)
            if nm == 'ObjectsList':
                walk(p + 10, p + 10 + size)
            elif nm == 'Object':
                c = d[p + 10:p + 10 + size]
                x, y, z, ang = struct.unpack_from('<4f', c, 0)
                flags = struct.unpack_from('<i', c, 16)[0]
                l = struct.unpack_from('<H', c, 20)[0]
                tmpl = c[22:22 + l].decode('latin1')
                props, _ = read_dict(c, 22 + l, names)
                out.append((tmpl, x, y, z, ang, flags, props))
            p += 10 + size
    walk(p, len(d))
    return out

if __name__ == '__main__':
    pat = re.compile(sys.argv[2], re.I) if len(sys.argv) > 2 else None
    for t, x, y, z, a, f, pr in objects(sys.argv[1]):
        if pat and not pat.search(t): continue
        keep = {k: v for k, v in pr.items() if k in ('originalOwner', 'objectName', 'waypointName', 'uniqueID') or 'Castle' in k or 'castle' in k}
        print('%-32s %8.1f %8.1f %6.1f ang %5.2f %s' % (t, x, y, z, a, keep))
