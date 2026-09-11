# 12 — Reverse engineering cookbook

Everything in this KB was produced on Linux with no Windows install. This is
how to keep going.

## Environment

| Thing | Where |
|---|---|
| Ghidra | `/opt/ghidra` (12.1.2) |
| Java | `/usr/lib/jvm/java-26-openjdk` — Ghidra 12 needs JDK 21+, the system default 17 is too old |
| PyGhidra | in the `pyghidra-mcp` tool venv: `~/.local/share/uv/tools/pyghidra-mcp/bin/python` |
| Ghidra project | `re/ghidra-proj/wddm.rep`, built by `tools/re/build_workspace.sh` |
| MCP server | registered as `ghidra` (`claude mcp list`) |

The MCP server and the headless scripts share one project. Ghidra locks a
project, so **do not run `tools/re/import.py` while the MCP server is live** — one
of them will fail to open it.

## Getting more binaries

`tools/re/msfetch.py` resolves a filename plus a Windows version through
[winbindex](https://winbindex.m417z.com) to a `(timestamp, virtualSize)` key,
downloads the PE from `msdl.microsoft.com`, parses its CodeView record, and
pulls the matching PDB.

```bash
python3 tools/re/msfetch.py list dxgkrnl.sys --winver 11-24H2
python3 tools/re/msfetch.py get  dxgmms2.sys --sha256 <hash> \
        --outdir re/bin --pdbdir re/bin
python3 tools/re/msfetch.py get  dxgkrnl.sys --winver 11-25H2 --pick latest \
        --outdir re/bin-25h2 --pdbdir re/bin-25h2   # for diffing across releases
```

Windows version keys that exist: `11-21H2` `11-22H2` `11-23H2` `11-24H2`
`11-25H2` `11-26H1`, plus every Windows 10 release.

Ghidra finds a PDB when it sits beside the binary with the *same basename*;
`tools/re/build_workspace.sh` renames each PDB to match. To add a binary to the
workspace, append its file name and SHA-256 (from `msfetch.py list`) to
`tools/re/binaries.txt` and re-run the script.

**Vendor drivers have no PDBs.** AMD's `amdkmdag.sys` and NVIDIA's
`nvlddmkm.sys` come from the vendor installer packages (or the Microsoft Update
Catalog, which is scriptable and needs no vendor account). Analysis there is
unassisted, but the DDI table gives a strong foothold: find `DriverEntry`, find
the `DRIVER_INITIALIZATION_DATA` it fills, and every slot index maps to a name
via `docs/ref/driver_init_table.md`. That single table converts an unnamed 50 MB
binary into 187 named entry points.

## Importing and querying

```bash
export GHIDRA_INSTALL_DIR=/opt/ghidra JAVA_HOME=/usr/lib/jvm/java-26-openjdk
PY=~/.local/share/uv/tools/pyghidra-mcp/bin/python

$PY tools/re/import.py re/ghidra-proj wddm re/bin/dxgmms2.sys

$PY tools/re/q.py re/ghidra-proj wddm dxgkrnl.sys count
$PY tools/re/q.py re/ghidra-proj wddm dxgkrnl.sys funcs 'VirtualGpu'
$PY tools/re/q.py re/ghidra-proj wddm basicrender.sys dec WarpKMSubmitCommandVirtual
$PY tools/re/q.py re/ghidra-proj wddm dxgkrnl.sys xref SubmitCommand
$PY tools/re/q.py re/ghidra-proj wddm dxgkrnl.sys strings 'vail'
```

Analysis emits a harmless `NullPointerException` from
`WindowsResourceReferenceAnalyzer` — it wants a script bundle host that only
exists in the GUI. Ignore it; the program is analyzed correctly.

## Symbols without Ghidra

Often faster. `llvm-pdbutil` reads MS PDBs directly:

```bash
llvm-pdbutil dump --summary  re/bin/dxgkrnl.pdb     # note: "Is stripped: true"
llvm-pdbutil dump --publics  re/bin/dxgkrnl.pdb | grep S_PUB32 | wc -l

python3 tools/re/pdbsyms.py re/bin/dxgkrnl.pdb 'Doorbell|NativeFence'
python3 tools/re/pdbsyms.py re/bin/dxgmms2.pdb 'Paging|Evict|Residen'
```

**Mangled C++ names carry parameter types even when the type stream is
stripped.** This is how the GPU-PV protocol in `09-gpupv.md` was recovered:
`?DdiPrepareLiveMigration@ADAPTER_RENDER@@QEAAJPEAU_DXGKARG_GPUP_PREPARE_LIVE_MIGRATION@@@Z`
names the argument struct even though its layout is absent. Demangle with
`llvm-cxxfilt --format=auto` or read them directly — MSVC mangling puts
`PEAU_NAME@@` for `struct _NAME *`.

## Regenerating the reference tables

```bash
python3 tools/re/ddi_extract.py third_party/windows-kit
# -> docs/ref/driver_init_table.md, docs/ref/ddi_by_version.json
```

To see what the next WDDM revision adds, change the WDK and SDK pins in
`tools/fetch_windows_kit.sh` to the newer packages, fetch into a scratch
`AV_WINDOWS_KIT_DIR`-style directory, and run `ddi_extract.py` against it and
diff — that is how you get early warning of DDI changes.

## High-value targets not yet analyzed

| Target | Why |
|---|---|
| `dxgmms2.sys` — `VidMm` paging | Exactly how VidMm sequences `BuildPagingBuffer` operations, and what it does when a KMD returns short. Imported, 2656 functions, symbolized. |
| `dxgmms2.sys` — `VidSch` | Preemption timing, TDR watchdog behaviour, node arbitration. |
| `dxgkrnl.sys` — `DXG_HOST_VIRTUALGPU_VMBUS::VmBusCommandTableVgpuToHost` | The literal dispatch table for all 92 GPU-PV messages, with handler addresses. Decompiling it recovers each message's handler and, from the handler, its payload shape. |
| `dxgkrnl.sys` — doorbell path | `Doorbell`-named functions show how `dxgkrnl` validates and maps a doorbell page. Directly relevant to phase 6. |
| `d3d12core.dll` | Which UMD DDI entries the runtime actually calls, in what order, at device creation. Answers "what is the minimum UMD" for phase 7. |
| `vpci.sys` | How a synthetic PCI device gets enumerated in a guest — relevant if `rvkgpu` is to bind to a virtio-pci device rather than a root-enumerated one. |
| `amdkmdag.sys` | A real, shipping, WDDM 3.2 KMD. No symbols, but the 193-field table makes it navigable. Worth it once a specific question needs a real-hardware answer. |

## A note on what RE is and is not good for here

The DDI *contract* is in the WDK headers and is normative — read those first,
always. Reverse engineering answers a different class of question:

- What does `dxgkrnl` do when I return X? (`dxgkrnl.sys`)
- What is the minimum I can get away with? (`basicrender.sys`)
- How did Microsoft solve this exact problem? (GPU-PV in `dxgkrnl.sys`)
- What does a real driver actually fill in? (`amdkmdag.sys`)

Reaching for the disassembler to answer "what are the fields of
`DXGKARG_CREATEALLOCATION`" is a waste — it is in `d3dkmddi.h`.
