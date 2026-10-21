#!/usr/bin/env python3
"""Carry function names from one build to another (WorldBuilder.exe -> game.dat).

Both binaries were compiled from the same BFME2X engine source, so most engine
functions exist in both with the same instruction shape. Matching proceeds:

  1. anchors   - functions whose mnemonic-hash (+size class) is unique in both
  2. strings   - functions that reference the same set of >=2 distinctive strings
  3. callgraph - propagate: if A matches A', unmatched callees of A at the same
                 call position with identical hashes are matched; repeat to a fixpoint

Only names that are not Ghidra defaults (FUN_*) are transferred.

Usage:
    match_functions.py src_features.jsonl dst_features.jsonl -o labels_matched.json
         [--src-labels labels_strings_wb.json]
"""
import argparse
import collections
import json
import sys


def load(path):
    fs = [json.loads(l) for l in open(path)]
    return {f["ep"]: f for f in fs}


def key(f):
    return (f["mhash"], f["ninsn"])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--src-labels", nargs="*", default=[], help="extra names for src functions (labels_*.json)")
    ap.add_argument("--min-insns", type=int, default=8)
    a = ap.parse_args()

    src, dst = load(a.src), load(a.dst)
    names = {ep: f["name"] for ep, f in src.items()}
    for lf in a.src_labels:
        for l in json.load(open(lf)):
            names[f'{l["va"]:08x}'] = l["name"]

    match = {}  # src ep -> (dst ep, method)
    taken = set()

    def add(s, d, how):
        if s in match or d in taken:
            return False
        match[s] = (d, how)
        taken.add(d)
        return True

    # 1. unique shape anchors
    sk, dk = collections.defaultdict(list), collections.defaultdict(list)
    for ep, f in src.items():
        if f["ninsn"] >= a.min_insns:
            sk[key(f)].append(ep)
    for ep, f in dst.items():
        if f["ninsn"] >= a.min_insns:
            dk[key(f)].append(ep)
    for k, ss in sk.items():
        ds = dk.get(k)
        if len(ss) == 1 and ds and len(ds) == 1:
            add(ss[0], ds[0], "shape")

    # 2. string sets
    def strkey(f):
        s = tuple(sorted(x for x in f["strs"] if len(x) >= 6))
        return s if len(s) >= 2 else None
    ss_, ds_ = collections.defaultdict(list), collections.defaultdict(list)
    for ep, f in src.items():
        k = strkey(f)
        if k:
            ss_[k].append(ep)
    for ep, f in dst.items():
        k = strkey(f)
        if k:
            ds_[k].append(ep)
    for k, ss in ss_.items():
        ds = ds_.get(k)
        if len(ss) == 1 and ds and len(ds) == 1:
            add(ss[0], ds[0], "strings")

    # 3. call-graph propagation
    changed = True
    rounds = 0
    while changed and rounds < 20:
        changed, rounds = False, rounds + 1
        for s, (d, _) in list(match.items()):
            sc, dc = src[s]["calls"], dst[d]["calls"]
            if len(sc) != len(dc):
                continue
            for cs, cd in zip(sc, dc):
                if cs in src and cd in dst and cs not in match and cd not in taken:
                    if key(src[cs]) == key(dst[cd]) or (src[cs]["strs"] and src[cs]["strs"] == dst[cd]["strs"]):
                        changed |= add(cs, cd, "callgraph")

    labels, by = [], collections.Counter()
    for s, (d, how) in match.items():
        n = names.get(s, "")
        if not n or n.startswith("FUN_") or n.startswith("thunk_"):
            continue
        dn = dst[d]["name"]
        if not dn.startswith("FUN_"):
            continue  # already named in dst
        conf = {"shape": 0.8, "strings": 0.85, "callgraph": 0.7}[how]
        labels.append({"va": int(d, 16), "name": n, "kind": "function", "source": f"match-{how}",
                       "confidence": conf})
        by[how] += 1
    json.dump(labels, open(a.out, "w"), indent=1)
    print(f"matched {len(match)} of {len(dst)} dst functions "
          f"({', '.join(f'{k} {v}' for k, v in collections.Counter(h for _, h in match.values()).items())}); "
          f"{len(labels)} names transferred ({dict(by)})", file=sys.stderr)


if __name__ == "__main__":
    main()
