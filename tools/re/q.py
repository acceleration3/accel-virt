#!/usr/bin/env python3
import os, sys, re
os.environ.setdefault("GHIDRA_INSTALL_DIR", "/opt/ghidra")
os.environ.setdefault("JAVA_HOME", "/usr/lib/jvm/java-26-openjdk")
import pyghidra; pyghidra.start(verbose=False)
from ghidra.base.project import GhidraProject
from java.io import File

PROJDIR = os.path.abspath(sys.argv[1]); PROJNAME = sys.argv[2]
PROG = sys.argv[3]; CMD = sys.argv[4]; ARG = sys.argv[5] if len(sys.argv) > 5 else None

proj = GhidraProject.openProject(PROJDIR, PROJNAME, True)
prog = proj.openProgram("/", PROG, True)
fm = prog.getFunctionManager()

def funcs():
    return list(fm.getFunctions(True))

if CMD == "funcs":
    rx = re.compile(ARG, re.I) if ARG else None
    n = 0
    for f in funcs():
        nm = f.getName()
        if rx and not rx.search(nm): continue
        n += 1
        print(f"{f.getEntryPoint()}  {f.getBody().getNumAddresses():>6}  {nm}")
    print(f"-- {n} functions", file=sys.stderr)

elif CMD == "count":
    print(f"functions={fm.getFunctionCount()}")
    st = prog.getSymbolTable()
    print(f"symbols={st.getNumSymbols()}")

elif CMD == "dec":
    from ghidra.app.decompiler import DecompInterface
    from ghidra.util.task import ConsoleTaskMonitor
    di = DecompInterface(); di.openProgram(prog)
    tgt = []
    for want in ARG.split(","):
        tgt += ([f for f in funcs() if f.getName() == want] or
                [f for f in funcs() if want.lower() in f.getName().lower()])[:4]
    for f in tgt:
        r = di.decompileFunction(f, 120, ConsoleTaskMonitor())
        print(f"//===== {f.getName()} @ {f.getEntryPoint()}")
        print(r.getDecompiledFunction().getC() if r.decompileCompleted() else "// FAILED")

elif CMD == "xref":
    tgt = [f for f in funcs() if ARG.lower() in f.getName().lower()]
    for f in tgt[:6]:
        print(f"== callers of {f.getName()}")
        for c in f.getCallingFunctions(None): print(f"   {c.getName()}")
        print(f"== callees")
        for c in f.getCalledFunctions(None): print(f"   {c.getName()}")

elif CMD == "strings":
    from ghidra.program.util import DefinedDataIterator
    rx = re.compile(ARG, re.I) if ARG else None
    for d in DefinedDataIterator.definedStrings(prog):
        s = str(d.getValue())
        if rx and not rx.search(s): continue
        print(f"{d.getAddress()}  {s}")

proj.close(prog); proj.close()
