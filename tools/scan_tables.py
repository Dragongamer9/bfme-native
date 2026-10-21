#!/usr/bin/env python3
"""Find every INI parse table in a SAGE executable and match it to Generals source.

SAGE stores INI schemas as static arrays in .rdata/.data:

    FieldParse  { const char* token; parseProc parse; const void* userData; int offset; }  (16 bytes)
    BlockParse  { const char* token; blockProc parse; }                                     ( 8 bytes)

terminated by an all-NULL entry. Those arrays survive into retail builds, so we can:
  1. find all of them by shape (no symbols needed),
  2. match each to its Generals counterpart by field-name overlap -> class name,
  3. vote parse-function pointers -> name INI::parseReal, INI::parseAsciiString, ...
  4. record field offsets -> the BFME memory layout of every *ModuleData class,
  5. list fields BFME added that Generals never had (the port's TODO list).

Usage:
    scan_tables.py game.dat --symbols data/generals_symbols.json -o data/
"""
import argparse
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sagepe import PE  # noqa: E402

MIN_ENTRIES = 2


def read_entry(pe, va, stride):
    tok = pe.u32(va)
    fn = pe.u32(va + 4)
    if tok is None or fn is None:
        return None
    if stride == 16:
        ud, off = pe.u32(va + 8), pe.u32(va + 12)
        if ud is None or off is None:
            return None
    else:
        ud, off = None, None
    return tok, fn, ud, off


def entry_ok(pe, e, stride):
    tok, fn, ud, off = e
    name = pe.ident(tok)
    if not name:
        return None
    if not pe.is_code(fn):
        return None
    if stride == 16 and off is not None and off > 0x100000 and off < 0xFFF00000:
        return None  # offsets are small (negative allowed for a few odd tables)
    return name


def is_terminator(e):
    return e is not None and e[0] == 0 and e[1] == 0


def find_tables(pe, stride):
    tables = []
    for s in pe.datasecs:
        va, end = s.va, s.end - stride
        while va < end:
            e = read_entry(pe, va, stride)
            name = e and entry_ok(pe, e, stride)
            if not name:
                va += 4
                continue
            start, rows = va, []
            while True:
                e = read_entry(pe, va, stride)
                name = e and entry_ok(pe, e, stride)
                if not name:
                    break
                rows.append({"va": va, "token": name, "parse": e[1],
                             "userData": e[2], "offset": e[3]})
                va += stride
            term = read_entry(pe, va, stride)
            if len(rows) >= MIN_ENTRIES and is_terminator(term):
                tables.append({"va": start, "stride": stride, "entries": rows})
                va += stride
            elif len(rows) == 0:
                va += 4
    return tables


def find_loose_blocks(pe, field_tables, ref_blocks):
    """RotWK no longer keeps one big BlockParse array: each subsystem owns a
    {token, parseProc} pair scattered through .data. Find those pairs, naming them
    exactly when Generals had the same keyword and tentatively when they sit among
    known ones (the pairs for one file cluster together)."""
    claimed = set()
    for t in field_tables:
        for e in t["entries"]:
            claimed.update(range(e["va"], e["va"] + 16, 4))
    gen = {}
    for r in ref_blocks:
        for e in r["entries"]:
            gen[e["token"]] = e["parser"]
    cand = []
    for s in pe.datasecs:
        for va in range(s.va, s.end - 8, 4):
            if va in claimed:
                continue
            tok, fn = pe.u32(va), pe.u32(va + 4)
            if not tok or not fn or not pe.is_code(fn):
                continue
            if pe.is_code(pe.u32(va - 4) or 0) or pe.is_code(pe.u32(va + 8) or 0):
                continue
            name = pe.ident(tok)
            if name and name[0].isupper() and "." not in name:
                cand.append((va, name, fn))
    known = [c for c in cand if c[1] in gen]
    kvas = [c[0] for c in known]
    out = []
    for va, name, fn in cand:
        if name in gen:
            out.append({"va": va, "token": name, "parse": fn, "generals_parser": gen[name], "confidence": 0.95})
        elif kvas and min(abs(va - k) for k in kvas) <= 0x80:
            out.append({"va": va, "token": name, "parse": fn,
                        "generals_parser": None, "confidence": 0.5})
    return out


def match_tables(found, ref_tables):
    """Assign each found table the Generals table it most resembles.

    Ranked by cosine overlap (so a full table beats a small subset table that it
    happens to contain, e.g. Object vs ObjectReskin), then accepted only if most
    of the smaller table's fields are present (BFME tables are often supersets).
    """
    ref_sets = [(r, set(e["token"] for e in r["entries"])) for r in ref_tables]
    for t in found:
        toks = set(e["token"] for e in t["entries"])
        best, rank, acc = None, 0.0, 0.0
        for r, rs in ref_sets:
            inter = len(toks & rs)
            if not inter:
                continue
            cos = inter / (len(rs) * len(toks)) ** 0.5
            if cos > rank:
                best, rank = r, cos
                acc = inter / min(len(rs), len(toks)) * min(1.0, inter / 3)
        if best and acc >= 0.5:
            t["match"] = {"class": best.get("class"), "function": best.get("function"),
                          "var": best["var"], "file": best["file"], "score": round(acc, 3),
                          "overlap": round(rank, 3)}
            ref_tokens = {e["token"]: e["parser"] for e in best["entries"]}
            for e in t["entries"]:
                e["generals_parser"] = ref_tokens.get(e["token"])
            t["new_in_bfme"] = [e["token"] for e in t["entries"] if e["token"] not in ref_tokens]
            t["removed_from_generals"] = [k for k in ref_tokens if k not in toks]


def vote_parsers(found, ref_tables):
    """Name parse-function pointers by majority vote across matched rows.

    Also votes globally by token: a field named "MaxHealth" anywhere in Generals
    used INI::parseReal, so an unmatched BFME table row "MaxHealth" is a vote too.
    """
    token_parser = collections.defaultdict(collections.Counter)
    for r in ref_tables:
        for e in r["entries"]:
            token_parser[e["token"]][e["parser"]] += 1
    votes = collections.defaultdict(collections.Counter)
    for t in found:
        for e in t["entries"]:
            if e.get("generals_parser"):
                votes[e["parse"]][e["generals_parser"]] += 3
            elif e["token"] in token_parser:
                p, n = token_parser[e["token"]].most_common(1)[0]
                if n == sum(token_parser[e["token"]].values()):
                    votes[e["parse"]][p] += 1
    labels = {}
    for va, c in votes.items():
        (name, n), total = c.most_common(1)[0], sum(c.values())
        if n >= 3 and n / total >= 0.75:
            labels[va] = {"name": name, "votes": n, "of": total}
    # a name may only own one address; keep the strongest
    by_name = {}
    for va, l in labels.items():
        if l["name"] not in by_name or l["votes"] > labels[by_name[l["name"]]]["votes"]:
            by_name[l["name"]] = va
    return {va: l for va, l in labels.items() if by_name[l["name"]] == va}


def table_label(t):
    m = t.get("match")
    if not m:
        return None
    if m["class"]:
        return f'{m["class"]}::{m["var"]}'
    return m["var"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("exe", help="RotWK game.dat / lotrbfme2ep1 game executable")
    ap.add_argument("--symbols", default="data/generals_symbols.json")
    ap.add_argument("-o", "--outdir", default="data")
    a = ap.parse_args()

    ref = json.load(open(a.symbols))
    pe = PE(a.exe)
    field = find_tables(pe, 16)
    claimed = {e["va"] for t in field for e in t["entries"]}
    block = [t for t in find_tables(pe, 8) if t["entries"][0]["va"] not in claimed
             and not any(e["va"] in claimed or e["va"] - 8 in claimed for e in t["entries"])]

    match_tables(field, ref["field_tables"])
    match_tables(block, ref["block_tables"])
    parser_labels = vote_parsers(field, ref["field_tables"])
    parser_labels.update(vote_parsers(block, ref["block_tables"]))

    loose = find_loose_blocks(pe, field, ref["block_tables"])
    labels = []
    seen_fn = set(parser_labels)
    for b in loose:
        if b["parse"] in seen_fn:
            continue
        seen_fn.add(b["parse"])
        name = b["generals_parser"] or f'INI::parse{b["token"]}Definition'
        labels.append({"va": b["parse"], "name": name, "kind": "function", "source": "block-entry",
                       "confidence": b["confidence"]})
        labels.append({"va": b["va"], "name": f'BlockParse_{b["token"]}', "kind": "data",
                       "source": "block-entry", "confidence": b["confidence"]})
    for va, l in sorted(parser_labels.items()):
        labels.append({"va": va, "name": l["name"], "kind": "function", "source": "fieldparse-vote",
                       "confidence": round(l["votes"] / l["of"], 2)})
    for t in field + block:
        lbl = table_label(t)
        if lbl:
            labels.append({"va": t["va"], "name": lbl, "kind": "data", "source": "table-match",
                           "confidence": t["match"]["score"]})
        else:
            labels.append({"va": t["va"], "name": f'FieldParseTable_{t["entries"][0]["token"]}_{t["va"]:08X}',
                           "kind": "data", "source": "table-shape", "confidence": 0.5})

    os.makedirs(a.outdir, exist_ok=True)
    out_tables = os.path.join(a.outdir, "tables.json")
    json.dump({"exe": os.path.basename(a.exe), "field_tables": field, "block_tables": block,
               "loose_blocks": loose},
              open(out_tables, "w"), indent=1)
    json.dump(labels, open(os.path.join(a.outdir, "labels_tables.json"), "w"), indent=1)

    matched = sum(1 for t in field if "match" in t)
    new_fields = sum(len(t.get("new_in_bfme", [])) for t in field)
    unmatched_fields = sum(len(t["entries"]) for t in field if "match" not in t)
    print(f"field tables: {len(field)} found, {matched} matched to Generals classes", file=sys.stderr)
    print(f"block tables: {len(block)} found, {sum(1 for t in block if 'match' in t)} matched", file=sys.stderr)
    print(f"INI block keywords: {sum(1 for b in loose if b['generals_parser'])} shared with Generals, "
          f"{sum(1 for b in loose if not b['generals_parser'])} BFME-only", file=sys.stderr)
    print(f"parse functions named: {len(parser_labels)}", file=sys.stderr)
    print(f"BFME-only fields in matched tables: {new_fields}; rows in unmatched (BFME-only) tables: "
          f"{unmatched_fields}", file=sys.stderr)


if __name__ == "__main__":
    main()
