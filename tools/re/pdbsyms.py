#!/usr/bin/env python3
import re, subprocess, sys
p = sys.argv[1]
out = subprocess.run(["llvm-pdbutil","dump","--publics",p],capture_output=True,text=True).stdout
syms=[]
for m in re.finditer(r"S_PUB32 \[size = \d+\] `([^`]+)`", out):
    syms.append(m.group(1))
if len(sys.argv)>2:
    rx=re.compile(sys.argv[2], re.I)
    syms=[s for s in syms if rx.search(s)]
for s in sorted(set(syms)): print(s)
print(f"-- {len(set(syms))}", file=sys.stderr)
