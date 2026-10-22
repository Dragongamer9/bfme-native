#!/usr/bin/env python3
"""List the CastleTemplates chunk of a RotWK base (.bse) or map: template, offset, angle, priority, phase.
   bseinfo.py FILE.bse"""
import struct, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mapinfo

def castle_templates(path):
    d = mapinfo.load(path)
    names, p = mapinfo.parse(d)
    while p + 10 <= len(d):
        cid, ver, size = struct.unpack_from('<IHi', d, p)
        if names.get(cid) == 'CastleTemplates':
            c = d[p + 10:p + 10 + size]
            q = 4  # property key (type byte + 3 byte name id)
            n = struct.unpack_from('<I', c, q)[0]; q += 4
            out = []
            for _ in range(n):
                l = struct.unpack_from('<H', c, q)[0]; q += 2
                name = c[q:q + l].decode('latin1'); q += l
                l = struct.unpack_from('<H', c, q)[0]; q += 2
                tmpl = c[q:q + l].decode('latin1'); q += l
                x, y, z, ang = struct.unpack_from('<4f', c, q); q += 16
                pri, phase = struct.unpack_from('<II', c, q); q += 8
                out.append((name, tmpl, x, y, z, ang, pri, phase))
            return ver, out, c[q:]
        p += 10 + size
    return None, [], b''

if __name__ == '__main__':
    ver, ents, rest = castle_templates(sys.argv[1])
    print('version', ver, 'entries', len(ents), 'trailing bytes', len(rest), rest[:64].hex(' '))
    for e in ents:
        print('%-12s %-36s %8.1f %8.1f %6.1f  ang %6.2f  pri %3d phase %d' % e)
