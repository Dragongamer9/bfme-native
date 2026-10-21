#!/usr/bin/env python3
"""For each top-level block in expanded INI files, compare where the engine's nesting rules
(tools/nesting_check.py) close it with the next column-0 "End". Reports the inner line where the
two first diverge.  Usage: toplevel_check.py <expanded.ini>..."""
import re, sys, os
sys.argv_saved = sys.argv; sys.argv = ["x"]
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nesting_check as nc
sys.argv = sys.argv_saved

def main():
    bad = 0
    for path in sys.argv[1:]:
        L = nc.parse(path)
        i = 0
        while i < len(L):
            ind, s = L[i]
            if ind == 0 and s and not s.lower().startswith("end"):
                eng = nc.skip_end(L, i)
                # oracle: the last End before the next column-0 block header
                nxt = next((j for j in range(i + 1, len(L)) if L[j][0] == 0 and L[j][1]
                            and not L[j][1].lower().startswith("end") and not L[j][1].startswith(("#", ";"))
                            and re.match(r"[A-Za-z]+\s+\S", L[j][1]) and "=" not in L[j][1]), len(L))
                col0 = next((j for j in range(nxt - 1, i, -1) if L[j][1].lower().startswith("end")), None)
                if col0 is not None and eng != col0:
                    bad += 1
                    if bad <= 30:
                        print(f"{os.path.basename(path)}:{i+1} {s[:40]!r}: rules close at {eng+1}, column-0 End at {col0+1}")
                i = (col0 if col0 is not None else eng) + 1
                continue
            i += 1
    print(f"{bad} top-level blocks disagree", file=sys.stderr)

main()
