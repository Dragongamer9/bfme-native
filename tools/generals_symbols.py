#!/usr/bin/env python3
"""Extract reference symbols from the GPL Generals/Zero Hour source.

These are the "Rosetta Stone" facts that survive into retail SAGE binaries
(including BFME2 / RotWK), because they are data, not debug code:

  * FieldParse tables   - INI field name -> parser function, per class
  * BlockParse table    - top-level INI block keyword -> parser ("Object", "Weapon", ...)
  * Module registry     - every addModule(Foo) in ModuleFactory::init

Usage:
    generals_symbols.py <GeneralsX checkout> -o data/generals_symbols.json
"""
import argparse
import json
import os
import re
import sys

TABLE_RE = re.compile(
    r"(?:static\s+)?(?:const\s+)?(FieldParse|BlockParse)\s+((?:\w+::)*\w+)\s*\[\s*\]\s*=\s*\{(.*?)\n\s*\}\s*;",
    re.S,
)
ENTRY_RE = re.compile(r'\{\s*"([^"]+)"\s*,\s*([A-Za-z_][\w:<>]*)')
FUNC_RE = re.compile(r"^[\w:<>\*&\s]*?\b(\w+)::(~?\w+)\s*\([^;{]*\)\s*(?:const\s*)?\{", re.M)
ADDMODULE_RE = re.compile(r"addModule\s*\(\s*(\w+)\s*\)")


def strip_comments(src: str) -> str:
    src = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


def enclosing_function(src: str, pos: int):
    best = None
    for m in FUNC_RE.finditer(src, 0, pos):
        best = m
    if best:
        return best.group(1), best.group(2)
    return None, None


def scan(root: str, game_dirs):
    field_tables, block_tables, modules = [], [], []
    for gd in game_dirs:
        base = os.path.join(root, gd)
        for dirpath, _, files in os.walk(base):
            for fn in files:
                if not fn.endswith((".cpp", ".h", ".inl")):
                    continue
                path = os.path.join(dirpath, fn)
                rel = os.path.relpath(path, root)
                try:
                    src = strip_comments(open(path, encoding="latin-1").read())
                except OSError:
                    continue
                for m in TABLE_RE.finditer(src):
                    kind, var, body = m.groups()
                    entries = [{"token": t, "parser": p} for t, p in ENTRY_RE.findall(body)]
                    if not entries:
                        continue
                    if "::" in var:
                        cls, func = var.rsplit("::", 1)[0], None
                        var = var.rsplit("::", 1)[1]
                    else:
                        cls, func = enclosing_function(src, m.start())
                    rec = {"file": rel, "var": var, "class": cls, "function": func, "entries": entries}
                    (field_tables if kind == "FieldParse" else block_tables).append(rec)
                if fn in ("ModuleFactory.cpp", "W3DModuleFactory.cpp"):
                    for name in ADDMODULE_RE.findall(src):
                        if name not in modules:
                            modules.append(name)
    return field_tables, block_tables, modules


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("root", help="GeneralsX (or any Generals source) checkout")
    ap.add_argument("-o", "--out", default="data/generals_symbols.json")
    ap.add_argument("--games", default="Core,GeneralsMD",
                    help="comma list of top-level dirs to scan (Zero Hour is closest to BFME)")
    a = ap.parse_args()

    ft, bt, mods = scan(a.root, a.games.split(","))
    # de-duplicate tables that appear in both Core and a game dir
    seen, uniq = set(), []
    for t in ft:
        key = (t["class"], t["var"], tuple(e["token"] for e in t["entries"]))
        if key not in seen:
            seen.add(key)
            uniq.append(t)
    out = {"field_tables": uniq, "block_tables": bt, "modules": mods}
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    json.dump(out, open(a.out, "w"), indent=1)
    nfields = sum(len(t["entries"]) for t in uniq)
    print(f"{len(uniq)} field tables ({nfields} fields), {len(bt)} block tables, "
          f"{len(mods)} modules -> {a.out}", file=sys.stderr)


if __name__ == "__main__":
    main()
