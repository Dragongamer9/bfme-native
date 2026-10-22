#!/usr/bin/env python3
"""Print the chunk tree of a W3D file (ids, sizes, mesh/hierarchy names).
   w3dchunks.py FILE.w3d [maxdepth]"""
import struct, sys
NAMES = {0x0: 'MESH', 0x1F: 'MESH_HEADER3', 0x2: 'VERTICES', 0x3: 'VERTEX_NORMALS', 0x20: 'TRIANGLES', 0x22: 'VERTEX_SHADE_INDICES',
         0x28: 'MATERIAL_INFO', 0x29: 'SHADERS', 0x2A: 'VERTEX_MATERIALS', 0x30: 'TEXTURES', 0x38: 'MATERIAL_PASS', 0x58: 'DEFORM',
         0x80: 'PS2_SHADERS', 0x90: 'AABTREE', 0x100: 'HIERARCHY', 0x200: 'ANIMATION', 0x280: 'COMPRESSED_ANIMATION',
         0x300: 'MORPH_ANIMATION', 0x400: 'HMODEL', 0x500: 'LODMODEL', 0x600: 'COLLECTION', 0x700: 'POINTS', 0x800: 'LIGHT',
         0x900: 'EMITTER', 0xA00: 'AGGREGATE', 0xB00: 'HLOD', 0xC00: 'BOX', 0xC01: 'SPHERE', 0xC02: 'RING', 0xD00: 'NULL_OBJECT',
         0xE00: 'LIGHTSCAPE', 0xF00: 'DAZZLE', 0xF01: 'DAZZLE_NAME', 0xF02: 'SOUNDROBJ', 0x60: 'TANGENTS', 0x61: 'BINORMALS',
         0x50: 'FX_SHADERS', 0x51: 'FX_SHADER', 0x52: 'FX_SHADER_INFO', 0x53: 'FX_SHADER_CONSTANT', 0x16: 'VERTEX_INFLUENCES',
         0x0C: 'MESH_USER_TEXT', 0x24: 'VERTEX_COLORS'}
def walk(d, p, end, depth, maxd):
    while p + 8 <= end:
        cid, size = struct.unpack_from('<II', d, p)
        sub = size & 0x80000000; size &= 0x7fffffff
        name = NAMES.get(cid, '?%#x' % cid)
        extra = ''
        if cid == 0x1F: extra = d[p + 8 + 8:p + 8 + 8 + 16].split(b'\0')[0].decode('latin1') + '.' + d[p + 8 + 24:p + 8 + 40].split(b'\0')[0].decode('latin1')
        print('%s%s %d%s' % ('  ' * depth, name, size, ' ' + extra if extra else ''))
        if sub and depth < maxd:
            walk(d, p + 8, p + 8 + size, depth + 1, maxd)
        p += 8 + size
d = open(sys.argv[1], 'rb').read()
walk(d, 0, len(d), 0, int(sys.argv[2]) if len(sys.argv) > 2 else 1)
