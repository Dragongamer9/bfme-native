#!/usr/bin/env python3
"""Merge labels_*.json into one labels.json (+ CSV), resolving conflicts by confidence.

Usage: merge_labels.py data/        -> data/labels.json, data/labels.csv
"""
import glob
import json
import os
import sys


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else "data"
    best, names = {}, {}
    for f in sorted(glob.glob(os.path.join(d, "labels_*.json"))):
        for l in json.load(open(f)):
            cur = best.get(l["va"])
            if cur is None or l["confidence"] > cur["confidence"]:
                best[l["va"]] = l
    # a symbol name may only appear once; suffix duplicates with their address
    out = []
    for va in sorted(best):
        l = dict(best[va])
        if l["name"] in names:
            l["name"] = f'{l["name"]}_{va:08X}'
        names[l["name"]] = va
        out.append(l)
    json.dump(out, open(os.path.join(d, "labels.json"), "w"), indent=1)
    with open(os.path.join(d, "labels.csv"), "w") as fh:
        fh.write("address,name,kind,source,confidence\n")
        for l in out:
            fh.write(f'{l["va"]:08X},{l["name"]},{l["kind"]},{l["source"]},{l["confidence"]}\n')
    by = {}
    for l in out:
        by[l["source"]] = by.get(l["source"], 0) + 1
    print(f"{len(out)} labels: " + ", ".join(f"{k} {v}" for k, v in sorted(by.items())), file=sys.stderr)


if __name__ == "__main__":
    main()
