#!/usr/bin/env python3
"""Recover the SAGE module registry (every Behavior/Update/Body/Draw/... class).

ModuleFactory::init() is a long run of

    addModule( ActiveBody );   ->   addModuleInternal(ActiveBody::friend_newModuleInstance,
                                                      ActiveBody::friend_newModuleData,
                                                      type, AsciiString("ActiveBody"), intf);

In the retail binary that compiles to: a reference to the string "ActiveBody",
two pushed code pointers (the factories), and a call to addModuleInternal.
From one function we therefore get, for every module class in the game -
including all the ones BFME added - its name, its two factory functions, and
(by walking into the constructor) its vtable.

Usage:
    scan_modules.py game.dat --symbols data/generals_symbols.json -o data/
"""
import argparse
import collections
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sagepe import PE  # noqa: E402

HEX = re.compile(r"0x[0-9a-f]+")


def imm_values(ins):
    return [int(t, 16) for t in HEX.findall(ins.op_str)]


def locate_init(pe, module_names):
    vas = {}
    for n in module_names:
        va = pe.string_va(n)
        if va:
            vas[va] = n
    if not vas:
        raise SystemExit("none of the Generals module names occur in this exe")
    xrefs = pe.code_xrefs_to(vas.keys())
    sites = sorted(a for refs in xrefs.values() for a in refs)
    if not sites:
        raise SystemExit("module name strings found but never referenced from code")
    # cluster reference sites; the registry is one huge function
    clusters, cur = [], [sites[0]]
    for a in sites[1:]:
        if a - cur[-1] < 0x800:
            cur.append(a)
        else:
            clusters.append(cur)
            cur = [a]
    clusters.append(cur)
    # the logic registry (ModuleFactory) and the W3D draw registry (W3DModuleFactory) are separate functions
    regs = sorted((c for c in clusters if len(c) >= 3), key=len, reverse=True) or [max(clusters, key=len)]
    return [(c[0], c[-1], len(c)) for c in regs], len(vas)


def walk_registry(pe, lo, hi):
    """Linear walk from the first reference to past the last; returns raw records."""
    insns = []
    for ins in pe.disasm(lo, max_insns=200000, stop_at_ret=False):
        insns.append(ins)
        if ins.address > hi and ins.mnemonic in ("ret", "retn"):
            break
        if ins.address > hi + 0x4000:
            break
    # find addModuleInternal: the call target most often preceded by 2 code-pointer pushes
    pushes, counts = [], collections.Counter()
    for ins in insns:
        if ins.mnemonic == "push":
            v = imm_values(ins)
            if v and pe.is_code(v[0]):
                pushes.append(v[0])
        elif ins.mnemonic == "call":
            v = imm_values(ins)
            if v and len(pushes) >= 2:
                counts[v[0]] += 1
            pushes = []
    if not counts:
        raise SystemExit("could not identify addModuleInternal")
    add_internal = counts.most_common(1)[0][0]

    records, name, code_ptrs = [], None, []
    for ins in insns:
        if ins.mnemonic == "call" and imm_values(ins)[:1] == [add_internal]:
            if name and len(code_ptrs) >= 2:
                records.append({"name": name, "newModuleInstance": code_ptrs[-1],
                                "newModuleData": code_ptrs[-2], "site": ins.address})
            name, code_ptrs = None, []
            continue
        for v in imm_values(ins):
            if ins.mnemonic == "push" and pe.is_code(v):
                code_ptrs.append(v)
            elif pe.is_data(v):
                s = pe.ident(v)
                if s and s[0].isupper():
                    name = s
    return add_internal, records, insns[0].address


def vtable_len(pe, va, limit=400, stop=()):
    """Count consecutive code pointers; stops at another known vtable (they are packed)."""
    n = 0
    while n < limit:
        if n and (va + 4 * n) in stop:
            break
        p = pe.u32(va + 4 * n)
        if p is None or not pe.is_code(p):
            break
        n += 1
    return n


def vtable_stores(pe, fn, max_insns=600):
    out = []
    for ins in pe.disasm(fn, max_insns=max_insns):
        if ins.mnemonic != "mov" or not ins.op_str.startswith("dword ptr ["):
            continue
        v = imm_values(ins)
        if not v:
            continue
        cand = v[-1]
        if pe.is_data(cand) and vtable_len(pe, cand) >= 2:
            out.append(cand)
    return out


def find_ctor_vtable(pe, factory):
    own = vtable_stores(pe, factory, 300)
    if own:
        return None, own[-1]
    best = (None, None)
    for ins in pe.disasm(factory, max_insns=300):
        if ins.mnemonic in ("call", "jmp"):  # jmp: tail-called constructor
            v = imm_values(ins)
            if v and pe.is_code(v[0]):
                st = vtable_stores(pe, v[0])
                if st:
                    best = (v[0], st[-1])
    return best


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("exe")
    ap.add_argument("--symbols", default="data/generals_symbols.json")
    ap.add_argument("-o", "--outdir", default="data")
    a = ap.parse_args()

    ref = json.load(open(a.symbols))
    pe = PE(a.exe)
    regs, nstr = locate_init(pe, ref["modules"])
    recs, adds, starts = [], [], []
    for lo, hi, nrefs in regs:
        try:
            add_internal, r, walk_start = walk_registry(pe, lo, hi)
        except SystemExit:
            continue
        if len(r) < 5 and recs:  # small clusters are usually stray string uses, except the first
            continue
        print(f"registry at {lo:#x}: {len(r)} modules ({nrefs} refs to known names); "
              f"addModuleInternal={add_internal:#x}", file=sys.stderr)
        seen = {x["name"] for x in recs}
        recs += [x for x in r if x["name"] not in seen]
        adds.append(add_internal)
        starts.append(walk_start)
    add_internal = adds[0]

    generals = set(ref["modules"])
    inst_count = collections.Counter(r["newModuleInstance"] for r in recs)
    data_count = collections.Counter(r["newModuleData"] for r in recs)
    labels = [{"va": a_, "name": "ModuleFactory::addModuleInternal", "kind": "function",
               "source": "module-registry", "confidence": 0.95} for a_ in sorted(set(adds))]
    vt_owner = {}
    for r in recs:
        r["in_generals"] = r["name"] in generals
        ctor, vt = find_ctor_vtable(pe, r["newModuleInstance"])
        r["ctor"], r["vtable"] = ctor, vt
        r["vtable_len"] = vtable_len(pe, vt) if vt else 0
        if inst_count[r["newModuleInstance"]] == 1:
            labels.append({"va": r["newModuleInstance"], "name": f'{r["name"]}::friend_newModuleInstance',
                           "kind": "function", "source": "module-registry", "confidence": 0.95})
        if data_count[r["newModuleData"]] == 1:
            labels.append({"va": r["newModuleData"], "name": f'{r["name"]}::friend_newModuleData',
                           "kind": "function", "source": "module-registry", "confidence": 0.9})
        if ctor:
            labels.append({"va": ctor, "name": f'{r["name"]}::{r["name"]}', "kind": "function",
                           "source": "module-ctor", "confidence": 0.7})
        if vt:
            vt_owner.setdefault(vt, []).append(r["name"])

    # vtables + their unique slots
    known = set(vt_owner)
    for r in recs:
        if r["vtable"]:
            r["vtable_len"] = vtable_len(pe, r["vtable"], stop=known)
    slot_users = collections.defaultdict(set)
    for vt, owners in vt_owner.items():
        for i in range(vtable_len(pe, vt, stop=known)):
            slot_users[pe.u32(vt + 4 * i)].add((vt, i))
    for vt, owners in vt_owner.items():
        if len(owners) != 1:
            continue
        cls = owners[0]
        labels.append({"va": vt, "name": f"{cls}::vftable", "kind": "data", "source": "module-ctor",
                       "confidence": 0.7})
        for i in range(vtable_len(pe, vt, stop=known)):
            fn = pe.u32(vt + 4 * i)
            if len(slot_users[fn]) == 1:
                labels.append({"va": fn, "name": f"{cls}::vf{i:02d}", "kind": "function",
                               "source": "vtable-slot", "confidence": 0.6})

    os.makedirs(a.outdir, exist_ok=True)
    json.dump({"exe": os.path.basename(a.exe), "addModuleInternal": adds,
               "registry_start": starts, "modules": recs},
              open(os.path.join(a.outdir, "modules.json"), "w"), indent=1)
    json.dump(labels, open(os.path.join(a.outdir, "labels_modules.json"), "w"), indent=1)

    new = [r["name"] for r in recs if not r["in_generals"]]
    print(f"modules: {len(recs)} total, {len(recs) - len(new)} shared with Generals, {len(new)} BFME-only; "
          f"vtables found: {sum(1 for r in recs if r['vtable'])}", file=sys.stderr)


if __name__ == "__main__":
    main()
