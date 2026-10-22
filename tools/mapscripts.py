#!/usr/bin/env python3
"""Dump the scripts of a RotWK map: groups, scripts, conditions and actions with parameters.
   mapscripts.py FILE.map [regex on script name or parameters]"""
import struct, sys, os, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mapinfo

def main(path, pat=None):
    d = mapinfo.load(path)
    names, p = mapinfo.parse(d)
    rx = re.compile(pat, re.I) if pat else None
    out = []
    def rstr(c, q):
        l = struct.unpack_from('<H', c, q)[0]; q += 2
        return c[q:q + l].decode('latin1'), q + l
    def params(c, q, n):
        ps = []
        for _ in range(n):
            t = struct.unpack_from('<i', c, q)[0]; q += 4
            if t == 16:  # coord3d
                x, y, z = struct.unpack_from('<3f', c, q); q += 12
                ps.append('(%.0f,%.0f)' % (x, y))
            else:
                iv, rv = struct.unpack_from('<if', c, q); q += 8
                s, q = rstr(c, q)
                ps.append(s if s else (str(iv) if iv else ('%g' % rv)))
        return ps, q
    def walk(p, end, depth):
        while p + 10 <= end:
            cid, ver, size = struct.unpack_from('<IHi', d, p)
            nm = names.get(cid, '?')
            c = d[p + 10:p + 10 + size]
            if nm in ('PlayerScriptsList',):
                walk(p + 10, p + 10 + size, depth)
            elif nm == 'ScriptList':
                out.append(('  ' * depth) + '[list]')
                walk(p + 10, p + 10 + size, depth + 1)
            elif nm == 'ScriptGroup':
                g, q = rstr(c, 0)
                out.append(('  ' * depth) + 'group %s active=%d' % (g, c[q]))
                q += 2 if ver >= 2 else 1
                walk(p + 10 + q, p + 10 + size, depth + 1)
            elif nm == 'Script':
                q = 0; strs = []
                for _ in range(4):
                    s, q = rstr(c, q); strs.append(s)
                flags = c[q:q + 6]; q += 6
                if ver >= 2: q += 4
                if ver >= 3:
                    q += 7; _, q = rstr(c, q)
                if ver >= 4: _, q = rstr(c, q)
                out.append(('  ' * depth) + 'script %s active=%d once=%d' % (strs[0], flags[0], flags[1]))
                walk(p + 10 + q, p + 10 + size, depth + 1)
            elif nm == 'OrCondition':
                out.append(('  ' * depth) + 'OR')
                walk(p + 10, p + 10 + size, depth + 1)
            elif nm in ('Condition', 'ScriptAction', 'ScriptActionFalse'):
                t = struct.unpack_from('<i', c, 0)[0]
                key = struct.unpack_from('<I', c, 4)[0]
                kname = names.get(key >> 8, names.get(key, '#%d' % key))
                n = struct.unpack_from('<i', c, 8)[0]
                try:
                    ps, _ = params(c, 12, n)
                except Exception as e:
                    ps = ['?']
                tag = {'Condition': 'if', 'ScriptAction': 'do', 'ScriptActionFalse': 'else'}[nm]
                out.append(('  ' * depth) + '%s %s(%s)' % (tag, kname, ', '.join(ps)))
            p += 10 + size
    while p + 10 <= len(d):
        cid, ver, size = struct.unpack_from('<IHi', d, p)
        if names.get(cid) == 'PlayerScriptsList':
            walk(p, p + 10 + size, 0)
        p += 10 + size
    if rx:
        # print scripts whose block matches
        block = []
        for line in out + ['script _end']:
            if line.strip().startswith(('script', 'group', '[list]')):
                if block and any(rx.search(b) for b in block):
                    print('\n'.join(block))
                block = [line]
            else:
                block.append(line)
    else:
        print('\n'.join(out))

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
