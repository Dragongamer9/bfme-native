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


def iter_ini_files(root):
    for dp, _, fns in os.walk(root):
        for fn in fns:
            if fn.lower().endswith((".ini", ".inc")):
                path = os.path.join(dp, fn)
                try:
                    with open(path, encoding="latin-1") as f:
                        raw = f.read().splitlines()
                except OSError:
                    continue
                lines = []
                for ln in raw:
                    body = COMMENT_RE.sub("", ln).rstrip()
                    if body.strip():
                        lines.append((len(body) - len(body.lstrip()), body.strip()))
                yield path, lines


def opens_block(lines, i):
    """BFME has 'Key = Value' lines that open a block (AnimationState = IDLE ... End).
    A '=' line opens a block when the next line is indented deeper."""
    ind, _ = lines[i]
    return i + 1 < len(lines) and lines[i + 1][0] > ind and lines[i + 1][1].lower() != "end"


def learn_eq_openers(files):
    """Keys whose 'Key = Value' lines open a block in most of their occurrences."""
    yes, tot = collections.Counter(), collections.Counter()
    for _, lines in files:
        for i, (ind, ln) in enumerate(lines):
            if ind == 0:
                continue  # top-level lines always open blocks
            k = ln.partition("=")[0].strip() if "=" in ln else ln.split(None, 1)[0]
            k = ("=" if "=" in ln else "") + k
            tot[k] += 1
            yes[k] += opens_block(lines, i)
    return {k for k in tot if tot[k] >= 5 and yes[k] / tot[k] >= 0.8}


def scan(root):
    files_l = list(iter_ini_files(root))
    eq_open = learn_eq_openers(files_l)
    blocks = collections.Counter()
    modules = collections.Counter()
    module_fields = collections.defaultdict(collections.Counter)
    block_fields = collections.defaultdict(collections.Counter)
    files = 0
    for path, lines in files_l:
        files += 1
        stack = []  # [(kind, name)]
        in_script = False
        for i, (ind, ln) in enumerate(lines):
            if ln.startswith("#"):
                continue
            if in_script:  # embedded Lua between BeginScript/EndScript has no INI structure
                in_script = ln.split(None, 1)[0].lower() != "endscript"
                continue
            if ln.split(None, 1)[0].lower() == "beginscript":
                in_script = True
                continue
            head = ln.split(None, 1)[0]
            low = head.lower()
            if low in ("end", "end_end"):
                if stack:
                    stack.pop()
                continue
            if "=" in ln:
                key, _, val = ln.partition("=")
                key, val = key.strip(), val.strip()
                if key.lower() in MODULE_KEYS and stack and stack[0][1] in ("Object", "ChildObject", "ObjectReskin"):
                    cls = val.split()[0] if val else "?"
                    modules[cls] += 1
                    stack.append(("module", cls))
                    continue
                owner = None
                for kind, name in reversed(stack):
                    if kind == "module":
                        owner = ("module", name)
                        break
                if owner:
                    module_fields[owner[1]][key] += 1
                elif stack:
                    block_fields[stack[-1][1]][key] += 1
                if "=" + key in eq_open:
                    stack.append(("sub", key))
                continue
            if not stack:
                blocks[head] += 1
                stack.append(("block", head))
            else:
                owner = next((n for k, n in reversed(stack) if k == "module"), None)
                if owner:
                    module_fields[owner][head] += 1
                else:
                    block_fields[stack[-1][1]][head] += 1
                if head in eq_open:  # e.g. ModelConditionState / ArmorSet open; ParticleSysBone does not
                    stack.append(("sub", head))
    return {"files": files, "blocks": blocks, "modules": modules,
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
