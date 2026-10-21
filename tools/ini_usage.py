#!/usr/bin/env python3
"""Measure exactly what a mod's INI data asks of the engine.

Walks an extracted data tree (vanilla RotWK + Edain + submod, overlaid with
bigtool.py) and counts:
  * top-level block types   (Object, Weapon, CommandButton, ...)
  * module classes used      (Behavior = HordeContain ..., Body = ..., Draw = ...)
  * fields set inside each module / block

Cross-referenced with the Generals source symbols (and, once available, the
RotWK exe scan), this becomes the port's scope list: every module the mod uses
that GeneralsX already implements, and every one we must write.

Usage:
    ini_usage.py <data dir> --symbols data/generals_symbols.json [--modules data/modules.json] -o data/ini_usage.json
"""
import argparse
import collections
import json
import os
import re
import sys

MODULE_KEYS = {"behavior", "body", "draw", "clientupdate", "clientbehavior", "ai", "update"}
COMMENT_RE = re.compile(r"(;|//).*$")


def iter_ini_lines(root):
    for dp, _, fns in os.walk(root):
        for fn in fns:
            if fn.lower().endswith(".ini") or fn.lower().endswith(".inc"):
                path = os.path.join(dp, fn)
                try:
                    with open(path, encoding="latin-1") as f:
                        for ln in f:
                            yield path, COMMENT_RE.sub("", ln).strip()
                except OSError:
                    continue


def scan(root):
    blocks = collections.Counter()
    modules = collections.Counter()
    module_fields = collections.defaultdict(collections.Counter)
    block_fields = collections.defaultdict(collections.Counter)
    files = set()
    stack = []  # [(kind, name)]
    cur_path = None
    for path, ln in iter_ini_lines(root):
        if path != cur_path:
            cur_path, stack = path, []
            files.add(path)
        if not ln or ln.startswith("#"):
            continue
        tok = ln.split(None, 1)
        head = tok[0]
        low = head.lower()
        if low == "end" or low == "end_end":
            if stack:
                stack.pop()
            continue
        if "=" in ln:
            key, _, val = ln.partition("=")
            key, val = key.strip(), val.strip()
            if key.lower() in MODULE_KEYS and stack:
                cls = val.split()[0] if val else "?"
                modules[cls] += 1
                stack.append(("module", cls))
                continue
            if stack:
                kind, name = stack[-1]
                (module_fields if kind == "module" else block_fields)[name][key] += 1
            continue
        # a line without '=' opens a block (top-level or nested, e.g. ArmorSet, WeaponSet, Draw states)
        if not stack:
            blocks[head] += 1
            stack.append(("block", head))
        else:
            parent = stack[-1][1]
            block_fields[parent][head] += 1
            stack.append(("sub", f"{parent}.{head}"))
    return {"files": len(files), "blocks": blocks, "modules": modules,
            "module_fields": module_fields, "block_fields": block_fields}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("datadir")
    ap.add_argument("--symbols", default="data/generals_symbols.json")
    ap.add_argument("--modules", help="modules.json from scan_modules.py (RotWK exe)")
    ap.add_argument("-o", "--out", default="data/ini_usage.json")
    a = ap.parse_args()

    r = scan(a.datadir)
    ref = json.load(open(a.symbols))
    in_generals = set(ref["modules"])
    in_exe = None
    if a.modules and os.path.exists(a.modules):
        in_exe = {m["name"] for m in json.load(open(a.modules))["modules"]}

    rows = []
    for cls, n in r["modules"].most_common():
        rows.append({"module": cls, "uses": n, "in_generals": cls in in_generals,
                     "in_rotwk_exe": (cls in in_exe) if in_exe is not None else None,
                     "fields": dict(r["module_fields"][cls].most_common())})
    out = {"datadir": os.path.abspath(a.datadir), "ini_files": r["files"],
           "blocks": dict(r["blocks"].most_common()), "modules": rows,
           "block_fields": {k: dict(v.most_common()) for k, v in r["block_fields"].items()}}
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    json.dump(out, open(a.out, "w"), indent=1)

    have = sum(1 for x in rows if x["in_generals"])
    uses_have = sum(x["uses"] for x in rows if x["in_generals"])
    uses_all = sum(x["uses"] for x in rows) or 1
    print(f"{r['files']} INI files, {len(r['blocks'])} block types, {len(rows)} module classes used", file=sys.stderr)
    print(f"module classes already in Generals source: {have}/{len(rows)} "
          f"({100 * uses_have / uses_all:.0f}% of module instances)", file=sys.stderr)
    print("top modules to port (not in Generals):", file=sys.stderr)
    for x in [x for x in rows if not x["in_generals"]][:15]:
        print(f"  {x['uses']:>6}  {x['module']}", file=sys.stderr)


if __name__ == "__main__":
    main()
