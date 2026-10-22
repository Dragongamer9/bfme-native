#!/usr/bin/env python3
"""Extract files from a folder of .big archives with game priority (first archive in name order wins).
   bigextract.py <big dir> <out dir> <regex on the lower-case path>"""
import os, re, struct, sys, glob

def entries(path):
    with open(path, 'rb') as fh:
        h = fh.read(16)
        if h[:4] not in (b'BIGF', b'BIG4'):
            return []
        n = struct.unpack('>I', h[8:12])[0]
        hs = struct.unpack('>I', h[12:16])[0]
        idx = fh.read(hs)
    out, p = [], 0
    for i in range(n):
        off, sz = struct.unpack('>II', idx[p:p + 8])
        e = idx.index(b'\0', p + 8)
        out.append((idx[p + 8:e].decode('latin1'), off, sz))
        p = e + 1
    return out

def main():
    src, dst, pat = sys.argv[1], sys.argv[2], re.compile(sys.argv[3])
    seen = set()
    n = 0
    for big in sorted(glob.glob(os.path.join(src, '*.big'))):
        for name, off, sz in entries(big):
            key = name.replace('\\', '/').lower()
            if key in seen or not pat.search(key):
                continue
            seen.add(key)
            # keep original case of the file name, normalise separators
            rel = name.replace('\\', '/')
            outp = os.path.join(dst, rel)
            os.makedirs(os.path.dirname(outp) or dst, exist_ok=True)
            with open(big, 'rb') as fh:
                fh.seek(off)
                data = fh.read(sz)
            with open(outp, 'wb') as fo:
                fo.write(data)
            n += 1
    print('extracted', n, 'files to', dst)

if __name__ == '__main__':
    main()
