#!/usr/bin/env python3
"""Name functions from the debug strings they reference.

SAGE code is full of messages such as
    "SidesList::xfer - script list missing/present mismatch"
    "INI::initFromINI - Invalid parameters supplied!"
and DEBUG_CRASH/ASSERT file paths such as
    "E:\\Builds\\BFME2X\\Code\\production\\Code\\GameEngine\\Source\\GameLogic\\System\\CastleSystem.cpp"
A function that references "Foo::bar - ..." almost always *is* Foo::bar; one that
references a .cpp path lives in that source file. WorldBuilder.exe was built from
the same engine source with these strings left in, so it names thousands of
functions that the game binary itself never mentions; match_functions.py then
carries those names across.

Usage:
    name_from_strings.py features.jsonl -o labels_strings.json [--files files.json]
"""
import argparse
import collections
import json
import re
import sys

METHOD_RE = re.compile(r"^([A-Za-z_]\w*(?:::[A-Za-z_~]\w*)+)\s*(?:-|:|\(\)|\(|$)")
FILE_RE = re.compile(r"[\\/]Code[\\/](.+\.(?:cpp|h|inl))$", re.I)
NOISE = {"std", "_STL", "stlp_std"}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("features")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--files", help="write {entry: source file} here")
    a = ap.parse_args()

    funcs = [json.loads(l) for l in open(a.features)]
    claims = collections.defaultdict(set)  # method name -> function eps
    per_fn = {}
    files = {}
    for f in funcs:
        names = collections.Counter()
        for s in f["strs"]:
            m = METHOD_RE.match(s)
            if m and m.group(1).split("::")[0] not in NOISE:
                names[m.group(1)] += 1
            fm = FILE_RE.search(s)
            if fm:
                files[f["ep"]] = fm.group(1).replace("\\", "/")
        if names:
            per_fn[f["ep"]] = names
            for n in names:
                claims[n].add(f["ep"])

    labels = []
    for ep, names in per_fn.items():
        # prefer a name only this function claims; with several, the most-mentioned one
        unique = [(c, n) for n, c in names.items() if len(claims[n]) == 1]
        if not unique:
            continue
        unique.sort(reverse=True)
        c, n = unique[0]
        conf = 0.85 if len(unique) == 1 else 0.7
        labels.append({"va": int(ep, 16), "name": n, "kind": "function", "source": "debug-string",
                       "confidence": conf})
    json.dump(labels, open(a.out, "w"), indent=1)
    if a.files:
        json.dump(files, open(a.files, "w"), indent=1)
    print(f"{len(funcs)} functions; {len(labels)} named from debug strings; "
          f"{len(files)} tied to a source file", file=sys.stderr)


if __name__ == "__main__":
    main()
