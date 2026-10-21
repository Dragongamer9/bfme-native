#!/usr/bin/env python3
"""List, extract and build SAGE .big archives (BIGF / BIG4).

Layout (Core/GameEngine/Source/Common/System/BIGFileSystem.cpp):
    char[4]  magic       "BIGF" (Generals, BFME) or "BIG4" (BFME2/RotWK patches, mods)
    u32 LE   archiveSize
    u32 BE   fileCount
    u32 BE   headerSize  (offset of first file's data)
    fileCount x { u32 BE offset; u32 BE size; char name[] (NUL-terminated, '\\' separators) }

Usage:
    bigtool.py list    Edain.big [--grep ini]
    bigtool.py extract Edain.big outdir/ [--grep data\\ini]
    bigtool.py pack    srcdir/ out.big [--magic BIG4]
    bigtool.py overlay out_dir/ a.big b.big ...   # later archives win, like the game's load order
"""
import argparse
import os
import struct
import sys


def read_index(path):
    with open(path, "rb") as f:
        hdr = f.read(16)
        if len(hdr) < 16 or hdr[:3] != b"BIG":
            raise SystemExit(f"{path}: not a .big archive")
        count, header_size = struct.unpack(">II", hdr[8:16])
        entries = []
        for _ in range(count):
            off, size = struct.unpack(">II", f.read(8))
            name = bytearray()
            while (c := f.read(1)) not in (b"\0", b""):
                name += c
            entries.append((name.decode("latin-1"), off, size))
    return hdr[:4].decode("latin-1"), entries


def safe_join(root, name):
    rel = name.replace("\\", "/").lstrip("/")
    out = os.path.normpath(os.path.join(root, rel))
    if not out.startswith(os.path.abspath(root) + os.sep) and out != os.path.abspath(root):
        raise SystemExit(f"refusing path outside output dir: {name}")
    return out


def extract(path, outdir, grep=None, quiet=False):
    _, entries = read_index(path)
    outdir = os.path.abspath(outdir)
    n = 0
    with open(path, "rb") as f:
        for name, off, size in entries:
            if grep and grep.lower() not in name.lower():
                continue
            dst = safe_join(outdir, name)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            f.seek(off)
            with open(dst, "wb") as o:
                o.write(f.read(size))
            n += 1
    if not quiet:
        print(f"{path}: extracted {n} files", file=sys.stderr)
    return n


def pack(srcdir, out, magic="BIGF"):
    files = []
    for dp, _, fns in os.walk(srcdir):
        for fn in fns:
            full = os.path.join(dp, fn)
            files.append((os.path.relpath(full, srcdir).replace("/", "\\"), full))
    files.sort(key=lambda x: x[0].lower())
    header_size = 16 + sum(8 + len(n.encode("latin-1")) + 1 for n, _ in files)
    offset, index = header_size, []
    for n, full in files:
        size = os.path.getsize(full)
        index.append((n, offset, size))
        offset += size
    with open(out, "wb") as o:
        o.write(magic.encode()[:4] + struct.pack("<I", offset) + struct.pack(">II", len(files), header_size))
        for n, off, size in index:
            o.write(struct.pack(">II", off, size) + n.encode("latin-1") + b"\0")
        for _, full in files:
            with open(full, "rb") as i:
                o.write(i.read())
    print(f"{out}: packed {len(files)} files ({offset} bytes)", file=sys.stderr)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("list"); p.add_argument("big"); p.add_argument("--grep")
    p = sub.add_parser("extract"); p.add_argument("big"); p.add_argument("outdir"); p.add_argument("--grep")
    p = sub.add_parser("pack"); p.add_argument("srcdir"); p.add_argument("out"); p.add_argument("--magic", default="BIGF")
    p = sub.add_parser("overlay"); p.add_argument("outdir"); p.add_argument("bigs", nargs="+")
    a = ap.parse_args()

    if a.cmd == "list":
        magic, entries = read_index(a.big)
        for name, off, size in entries:
            if not a.grep or a.grep.lower() in name.lower():
                print(f"{size:>10}  {name}")
        print(f"{magic}: {len(entries)} files", file=sys.stderr)
    elif a.cmd == "extract":
        extract(a.big, a.outdir, a.grep)
    elif a.cmd == "pack":
        pack(a.srcdir, a.out, a.magic)
    elif a.cmd == "overlay":
        for b in a.bigs:
            extract(b, a.outdir)


if __name__ == "__main__":
    main()
