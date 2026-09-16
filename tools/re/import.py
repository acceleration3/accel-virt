#!/usr/bin/env python3
"""Import PE binaries into a Ghidra project, apply the sibling PDB, analyze."""
import os, sys, pathlib
os.environ.setdefault("GHIDRA_INSTALL_DIR", "/opt/ghidra")
os.environ.setdefault("JAVA_HOME", "/usr/lib/jvm/java-26-openjdk")

import pyghidra
pyghidra.start(verbose=False)

from ghidra.base.project import GhidraProject
from ghidra.app.plugin.core.analysis import PdbUniversalAnalyzer
from ghidra.program.util import GhidraProgramUtilities
from ghidra.app.script import GhidraScriptUtil
from ghidra.util.task import ConsoleTaskMonitor
from ghidra.program.model.listing import Program
from java.io import File

PROJDIR = os.path.abspath(sys.argv[1])
PROJNAME = sys.argv[2]
targets = [os.path.abspath(x) for x in sys.argv[3:]]

pathlib.Path(PROJDIR).mkdir(parents=True, exist_ok=True)
try:
    proj = GhidraProject.openProject(PROJDIR, PROJNAME, True)
except Exception:
    proj = GhidraProject.createProject(PROJDIR, PROJNAME, False)

monitor = ConsoleTaskMonitor()
for t in targets:
    name = os.path.basename(t)
    try:
        existing = proj.openProgram("/", name, False)
        print(f"-- {name}: already in project, skipping")
        proj.close(existing)
        continue
    except Exception:
        pass
    print(f"++ {name}: importing")
    prog = proj.importProgram(File(t))
    pdb = os.path.splitext(t)[0] + ".pdb"
    if os.path.exists(pdb):
        PdbUniversalAnalyzer.setPdbFileOption(prog, File(pdb))
        print(f"   pdb -> {os.path.basename(pdb)}")
    proj.saveAs(prog, "/", name, True)
    from ghidra.program.flatapi import FlatProgramAPI
    txid = prog.startTransaction("analyze")
    try:
        from ghidra.app.plugin.core.analysis import AutoAnalysisManager
        mgr = AutoAnalysisManager.getAnalysisManager(prog)
        mgr.initializeOptions()
        mgr.reAnalyzeAll(None)
        mgr.startAnalysis(monitor)
    finally:
        prog.endTransaction(txid, True)
    GhidraProgramUtilities.markProgramAnalyzed(prog)
    proj.save(prog)
    proj.close(prog)
    print(f"   done {name}")
proj.close()
print("ALL DONE")
