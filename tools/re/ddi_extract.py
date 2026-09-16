#!/usr/bin/env python3
"""Extract the WDDM DDI surface: every DXGKDDI_* / DXGKCB_* prototype from the
WDK headers, tagged with the DXGKDDI_INTERFACE_VERSION that gates it, plus the
DRIVER_INITIALIZATION_DATA callback-table slot order.

  ddi_extract.py third_party/windows-kit   ->  docs/ref/{ddi_by_version.json,driver_init_table.md}
"""
import re, sys, json, collections, os

VER = re.compile(r"DXGKDDI_INTERFACE_VERSION\s*(>=|>|==)\s*(DXGKDDI_INTERFACE_VERSION_\w+)")
# a DDI prototype is `APIENTRY NAME(` or `(APIENTRY *NAME)(`
PROTO = re.compile(r"APIENTRY\s*\*?\s*\)?\s*(DXGKDDI_\w+|DXGKCB_\w+)\s*\)?\s*\(")
PROTO2 = re.compile(r"\(\s*APIENTRY\s*\*\s*(P?DXGKDDI_\w+|P?DXGKCB_\w+)\s*\)")

def scan(path):
    gate = {}
    stack = []          # (is_version_gate, version_or_None)
    for ln in open(path, encoding="utf-8", errors="replace"):
        s = ln.strip()
        if s.startswith("#if"):
            m = VER.search(s)
            stack.append(m.group(2) if m else None)
        elif s.startswith("#elif"):
            if stack:
                m = VER.search(s)
                stack[-1] = m.group(2) if m else None
        elif s.startswith("#endif"):
            if stack: stack.pop()
        active = [v for v in stack if v]
        cur = active[-1] if active else "BASE"
        for rx in (PROTO, PROTO2):
            for m in rx.finditer(ln):
                n = m.group(1).lstrip("P") if m.group(1).startswith("PDXGK") else m.group(1)
                gate.setdefault(n, cur)
    return gate

def init_table(path):
    src = open(path, encoding="utf-8", errors="replace").read()
    i = src.index("typedef struct _DRIVER_INITIALIZATION_DATA")
    j = src.index("} DRIVER_INITIALIZATION_DATA", i)
    body = src[i:j]
    slots, stack = [], []
    for ln in body.split("\n"):
        s = ln.strip()
        if s.startswith("#if"):
            m = VER.search(s); stack.append(m.group(2) if m else None); continue
        if s.startswith("#endif"):
            if stack: stack.pop()
            continue
        # VOID* / void* are the Reserved pointers; dropping them shifts every later slot.
        m = re.match(r"(?:__field\w*\s+)?(P?DXGKDDI_\w+|PDXGK\w+|ULONG|PVOID|VOID\s*\*|void\s*\*|DXGK\w+)\s+(\w+)\s*;", s)
        if m and m.group(1) not in ("ULONG",):
            active = [v for v in stack if v]
            slots.append((m.group(2), re.sub(r"\s+", "", m.group(1)), (active[-1] if active else "BASE").replace("DXGKDDI_INTERFACE_VERSION_","")))
        elif m:
            active = [v for v in stack if v]
            slots.append((m.group(2), re.sub(r"\s+", "", m.group(1)), (active[-1] if active else "BASE").replace("DXGKDDI_INTERFACE_VERSION_","")))
    return slots

ORDER = ["BASE","VISTA","VISTA_SP1","WIN7","WIN8","WDDM1_3","WDDM1_3_PATH_INDEPENDENT_ROTATION",
         "WDDM2_0","WDDM2_1","WDDM2_1_5","WDDM2_1_6","WDDM2_2","WDDM2_3","WDDM2_4","WDDM2_5",
         "WDDM2_6","WDDM2_7","WDDM2_8","WDDM2_9","WDDM3_0","WDDM3_1","WDDM3_2"]

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kit")
    parser.add_argument("--ref", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "docs", "ref"))
    args = parser.parse_args()
    REF = args.ref
    inc = os.path.join(args.kit, "include")
    gate = {}
    for h in ("wdk-shared/d3dkmddi.h", "km/dispmprt.h", "sdk-shared/d3dukmdt.h"):
        p = os.path.join(inc, h)
        if os.path.exists(p): gate.update(scan(p))
    by = collections.defaultdict(list)
    for k, v in gate.items():
        by[v.replace("DXGKDDI_INTERFACE_VERSION_", "")].append(k)
    tot = 0
    print("## DDI/CB surface introduced per WDDM level\n")
    for v in ORDER + sorted(set(by) - set(ORDER)):
        if v not in by: continue
        ddi = sorted(x for x in by[v] if x.startswith("DXGKDDI"))
        cb  = sorted(x for x in by[v] if x.startswith("DXGKCB"))
        tot += len(by[v])
        print(f"{v:<38} +{len(by[v]):>3} (ddi {len(ddi)}, cb {len(cb)})   cumulative {tot}")
    json.dump({k: sorted(v) for k, v in by.items()}, open(os.path.join(REF, "ddi_by_version.json"),"w"), indent=1)

    slots = init_table(os.path.join(inc, "km/dispmprt.h"))
    print(f"\n## DRIVER_INITIALIZATION_DATA: {len(slots)} slots")
    with open(os.path.join(REF, "driver_init_table.md"),"w") as f:
        f.write("| # | field | type | since |\n|--:|---|---|---|\n")
        for i,(n,t,v) in enumerate(slots):
            f.write(f"| {i} | `{n}` | `{t}` | {v} |\n")
    for n,t,v in slots[-14:]: print(f"  {n:<44} {v}")
