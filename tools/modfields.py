#!/usr/bin/env python3
"""Count the fields used inside every 'Behavior = <Module>' block of the INI dump.
   modfields.py <ModuleName> [inidump dir]"""
import glob, re, sys, collections
mod = sys.argv[1]
d = sys.argv[2] if len(sys.argv) > 2 else 'work/inidump'
c = collections.Counter(); ex = {}
for f in glob.glob(d + '/*.ini'):
    L = open(f, errors='replace').read().split('\n'); i = 0
    while i < len(L):
        if re.match(r'\s*(Behavior|Body|Draw|ClientUpdate|ClientBehavior)\s*=\s*%s\b' % mod, L[i]):
            i += 1; depth = 0
            while i < len(L):
                s = L[i].split(';')[0].strip()
                if re.match(r'End\b', s):
                    if depth == 0: break
                    depth -= 1
                elif s:
                    k = s.split('=')[0].strip().split()[0]
                    if '=' not in s: depth += 1
                    c[k] += 1; ex.setdefault(k, s)
                i += 1
        i += 1
for k, n in c.most_common():
    print('%5d %-32s %s' % (n, k, ex[k][:90]))
