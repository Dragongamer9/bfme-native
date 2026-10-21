#!/usr/bin/env python3
"""Turn the engine's BFME_INI_REPORT (count, kind, block, name) into a ranked work list.
Usage: skip_report.py data/rotwk/boot_report.tsv docs/SKIPPED.md"""
import collections, sys

rows = []
for ln in open(sys.argv[1], encoding="latin-1"):
    p = ln.rstrip("\n").split("\t")
    if len(p) == 4:
        rows.append((int(p[0]), p[1], p[2], p[3]))
by = collections.defaultdict(list)
for r in rows:
    by[r[1]].append(r)

def table(title, items, cols, n=40):
    out = [f"\n## {title}\n", "| " + " | ".join(cols) + " |", "|" + "---|" * len(cols)]
    for it in items[:n]:
        out.append("| " + " | ".join(str(x) for x in it) + " |")
    if len(items) > n:
        out.append(f"\n…and {len(items) - n} more.")
    return out

L = ["# What the engine still skips", "",
     "Generated from a full headless boot on RotWK 2.01 + Edain + Völker 4.0 (`tools/boot.sh`, "
     "`BFME_INI_REPORT`). Each line is something the original game understands that this engine "
     "does not yet; counts are occurrences in the data.", "",
     "| Kind | Distinct | Occurrences |", "|---|---:|---:|"]
for k in ("module", "field", "block", "value", "badfield", "strayEnd", "missing"):
    L.append(f"| {k} | {len(by[k])} | {sum(r[0] for r in by[k])} |")

mc = collections.Counter()
for c, _, _, n in by["module"]:
    mc[n] += c
mods = sorted(((c, n) for n, c in mc.items()), reverse=True)
L += table("Module classes not implemented (unit behaviours, draws, bodies)", [(n, c) for c, n in mods], ["Module", "Uses"], 60)
bc = collections.Counter()
for c, _, _, n in by["block"]:
    bc[n] += c
blocks = sorted(((c, n) for n, c in bc.items()), reverse=True)
L += table("Top-level INI block types not implemented", [(n, c) for c, n in blocks], ["Block", "Count"])
f = collections.Counter()
for c, _, blk, n in by["field"]:
    f[(blk, n)] += c
L += table("Fields not implemented (by block)", [(b, n, c) for (b, n), c in f.most_common()], ["Block", "Field", "Count"], 60)
bf = sorted(((c, b, n) for c, _, b, n in by["badfield"]), reverse=True)
L += table("Fields whose value format differs (parse errors)", [(b, n, c) for c, b, n in bf], ["Block", "Field", "Count"])
v = sorted(((c, b, n) for c, _, b, n in by["value"]), reverse=True)
L += table("Enum/flag values not known (in block)", [(b, n, c) for c, b, n in v], ["Block", "Value", "Count"], 40)
open(sys.argv[2], "w").write("\n".join(L) + "\n")
print(f"wrote {sys.argv[2]}")
