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

    # 4. relaxed propagation (different optimisation settings between the two builds):
    #    a) callees aligned by call order, sizes within a factor of ~1.7
    #    b) address-order neighbours: object files keep their function order at link time
    def similar(a, b):
        na, nb = a["ninsn"], b["ninsn"]
        if min(na, nb) < 4:
            return na == nb
        r = na / nb
        sa = set(x for x in a["strs"] if len(x) >= 4)
        sb = set(x for x in b["strs"] if len(x) >= 4)
        if sa and sb and not (sa & sb):
            return False
        return 0.6 <= r <= 1.7
    import difflib
    s_order = sorted(src, key=lambda e: int(e, 16))
    d_order = sorted(dst, key=lambda e: int(e, 16))
    s_idx = {e: i for i, e in enumerate(s_order)}
    d_idx = {e: i for i, e in enumerate(d_order)}
    for rnd in range(30):
        added = 0
        for s, (d, _) in list(match.items()):
            sc = [c for c in src[s]["calls"] if c in src]
            dc = [c for c in dst[d]["calls"] if c in dst]
            if sc and dc and len(sc) <= 200 and len(dc) <= 200:
                sm = difflib.SequenceMatcher(a=[src[c]["ninsn"] // 8 for c in sc],
                                             b=[dst[c]["ninsn"] // 8 for c in dc], autojunk=False)
                for blk in sm.get_matching_blocks():
                    for k in range(blk.size):
                        cs, cd = sc[blk.a + k], dc[blk.b + k]
                        if cs not in match and cd not in taken and similar(src[cs], dst[cd]):
                            added += add(cs, cd, "callgraph")
            for step in (-1, 1):
                i, j = s_idx[s] + step, d_idx[d] + step
                if 0 <= i < len(s_order) and 0 <= j < len(d_order):
                    ns, nd = s_order[i], d_order[j]
                    if ns not in match and nd not in taken and similar(src[ns], dst[nd]) \
                            and src[ns]["ninsn"] >= 6:
                        added += add(ns, nd, "neighbour")
        if not added:
            break

    labels, by = [], collections.Counter()
    for s, (d, how) in match.items():
        n = names.get(s, "")
        if not n or n.startswith("FUN_") or n.startswith("thunk_"):
            continue
        dn = dst[d]["name"]
        if not dn.startswith("FUN_"):
            continue  # already named in dst
        conf = {"shape": 0.8, "strings": 0.85, "callgraph": 0.65, "neighbour": 0.55}[how]
        labels.append({"va": int(d, 16), "name": n, "kind": "function", "source": f"match-{how}",
                       "confidence": conf})
        by[how] += 1
    json.dump(labels, open(a.out, "w"), indent=1)
    print(f"matched {len(match)} of {len(dst)} dst functions "
          f"({', '.join(f'{k} {v}' for k, v in collections.Counter(h for _, h in match.values()).items())}); "
          f"{len(labels)} names transferred ({dict(by)})", file=sys.stderr)


if __name__ == "__main__":
    main()
