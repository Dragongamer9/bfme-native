#!/usr/bin/env python3
"""Dump every INI enum/bit-flag name list RotWK uses, and diff it against Generals.

FieldParse rows parsed by INI::parseIndexList / parseBitString32 / parseByteSizedIndexList
carry their vocabulary in userData (a NULL-terminated const char* array). RotWK grew many of
these (new sound types, model condition flags, ...); every mismatch is a value Generals'
parser rejects, so this list is a checklist of enum arrays to update.

Usage: name_lists.py game.dat data/rotwk/tables.json data/rotwk/labels.json /path/to/GeneralsX -o data/rotwk/name_lists.json
"""
import argparse
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sagepe import PE  # noqa: E402

LIST_PARSERS = {"INI::parseIndexList", "INI::parseBitString32", "INI::parseBitString8",
                "INI::parseByteSizedIndexList", "INI::parseLookupList"}


def read_list(pe, va, limit=2048):
    names = []
    for i in range(limit):
        p = pe.u32(va + 4 * i)
        if not p:
            break
        s = pe.cstr(p, 200)
        if s is None:
            return None
        names.append(s.decode("latin-1"))
    return names


def generals_arrays(root):
    """name -> list for every `const char* ... Names[] = { "A", "B", ... }` in the source."""
    out = {}
    rx = re.compile(r"(?:const\s+)?char\s*\*\s*(?:const\s+)?(\w+)\s*\[\s*\w*\s*\]\s*=\s*\{(.*?)\};", re.S)
    for dp, _, fns in os.walk(root):
        if "/build" in dp:
            continue
        for fn in fns:
            if not fn.endswith((".cpp", ".h")):
                continue
            try:
                src = open(os.path.join(dp, fn), encoding="latin-1").read()
            except OSError:
                continue
            src = re.sub(r"//[^\n]*", "", src)
            for m in rx.finditer(src):
                vals = re.findall(r'"([^"]*)"', m.group(2))
                if len(vals) >= 2:
                    out.setdefault(m.group(1), vals)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("exe")
    ap.add_argument("tables")
    ap.add_argument("labels")
    ap.add_argument("generals")
    ap.add_argument("-o", "--out", required=True)
    a = ap.parse_args()

    pe = PE(a.exe)
    labels = {l["va"]: l["name"] for l in json.load(open(a.labels))}
    tables = json.load(open(a.tables))
    gen = generals_arrays(a.generals)
    gen_by_set = {tuple(v): k for k, v in gen.items()}

    lists = {}
    for t in tables["field_tables"]:
        owner = (t.get("match") or {}).get("class") or "?"
        for e in t["entries"]:
            if labels.get(e["parse"]) not in LIST_PARSERS or not e["userData"]:
                continue
            ud = e["userData"]
            if ud not in lists:
                names = read_list(pe, ud)
                if not names:
                    continue
                lists[ud] = {"va": ud, "names": names, "used_by": []}
            lists[ud]["used_by"].append(f'{owner}.{e["token"]}')

    report = []
    for ud, l in sorted(lists.items()):
        names = l["names"]
        # best Generals counterpart: the array sharing most names, preferring same prefix order
        best, score = None, 0
        for gname, gv in gen.items():
            common = len(set(gv) & set(names))
            if common > score:
                best, score = gname, common
        g = gen.get(best, [])
        same_prefix = names[:len(g)] == g
        l.update({"generals_array": best if score >= 2 else None,
                  "added": [n for n in names if n not in g] if best else names,
                  "removed": [n for n in g if n not in names] if best else [],
                  "generals_is_prefix": same_prefix})
        report.append(l)
    json.dump(report, open(a.out, "w"), indent=1)
    diff = [r for r in report if r["added"] or r["removed"]]
    print(f"{len(report)} name lists; {len(diff)} differ from Generals", file=sys.stderr)
    for r in diff:
        print(f'  {r["generals_array"] or "(new)":40} +{len(r["added"]):3} -{len(r["removed"]):3}  '
              f'{", ".join(r["used_by"][:3])}', file=sys.stderr)


if __name__ == "__main__":
    main()
