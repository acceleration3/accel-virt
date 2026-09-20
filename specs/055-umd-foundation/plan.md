# Implementation Plan: rvkumd.dll - the user-mode driver and DXGI

**Branch**: `055-umd-foundation` | **Date**: 2026-09-20 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/055-umd-foundation/spec.md`

## Summary

One freestanding `rvkumd.dll` that both Microsoft D3D runtimes load for the
rvkgpu adapter, named four times over in the adapter's `UserModeDriverName`
list because the kernel answers that list per Direct3D version. It exports
`OpenAdapter10_2` for `d3d11.dll` and `OpenAdapter12` for `D3D12Core.dll`
(research.md §1, read out of both binaries), answers the adapter function
tables those two hand back, reports the adapter's identity and memory from the
same LUID and video-memory queries the Vulkan ICD already makes, takes the
runtime's own memory for its device object, and fills the DXGI base function
table that arrives inside `CreateDevice` rather than through any export.

This is the foundation feature because all four of D3D9, 10, 11 and 12 share
exactly this: one DLL, one process connection, one adapter object per open, one
device object model in the runtime's memory, one DXGI seam, one device-removal
rule. What each runtime does *after* that - device functions, resources,
shaders, draws, feature levels - is 056, 057 and 064. So 055 finishes the seam
and answers the documented refusal on every slot beyond it, with a test per
slot, and claims no feature level at all (constitution VI).

Research moved four things out (research.md §11): the 32-bit arm to 061, D3D9
to 064, rendering and feature levels to 056/057, and FR-006's per-resource
private data to 056/057 - 055 builds the envelope every later record nests in,
so the KMD's refusal path is proven before the first field exists. Two things
it cannot finish are written as tasks and left unstarted: the rest of the
recorded runtime call sequences (FR-002 - research.md §12 has D3D12Core's
open and its first caps query, the D3D11 runtime's own order is still
unread) and the one-link-per-process join (FR-007, needs feature 053's
device-global object identity).

## Technical Context

**Language/Version**: C++23 freestanding (UMD core, shim); Python 3 (gate
scripts).

**Primary Dependencies**: 043/044's transport (`GuestConnection`,
`KmtTransport`, `AdapterSearch`, `Kmt`); 045's ICD substrate now in
`src/icd/common/` (`ThreadLock`, `LogSink`, `LockedTransport`) and its Windows
seams in `src/um/icd/` (`ProcessHeap`, `SrwLock`, `FileLog`); 045 T021's
`Kmt::adapter_luid` and `Kmt::query_video_memory`; 049's doorbell submission
path, for the device 056 will submit through; the WDK's `d3d10umddi.h`,
`d3d12umddi.h`, `d3dumddi.h` and the SDK's `dxgiddi.h`, which the kit does not
fetch today.

**Storage**: N/A.

**Testing**: L1 over `MockKmt`, `MockPlatform` and a new `FakeD3dRuntime` for
every UMD class - the fake resolves the adapter functions and calls them in the
order the real runtime does, the way `FakeOpengl32` does for feature 050;
hostile-input tables and a libFuzzer target for the private-data envelope and
for every DDI argument record the runtime hands in; the mirror records' offsets
proven against the real WDK headers in the Windows cross-build; L4 full-stack
through `DdiKmt` with the real driver behind it; L5 in the guest is written and
unstarted.

**Target Platform**: Windows 11 guest (x64 user mode, no CRT); Linux host.

**Project Type**: driver components (user-mode DLL, INF, gate scripts).

**Performance Goals**: none this feature can state - nothing is submitted yet.
The constraint that matters is that `OpenAdapter*` opens the process
connection once and every later open shares it.

**Constraints**: no STL/CRT; imports only gdi32, kernel32, ntdll; the DLL
loads, unloads and reloads cleanly, and its entry does nothing under the loader
lock; exactly the exports the two runtimes resolve and no others; nothing
reported that is not implemented and tested (constitution VI), which for 055
means no feature level and no D3D device.

**Scale/Scope**: one DLL, ~12 portable classes, ~10 mirror records, a fake
runtime and a rig in the harness, one `src/proto` envelope record with its fuzz
target, the INF's `UserModeDriverName` list, one kit-fetch change, one
`header_scan.py` change with its tools test.

## Constitution Check

| Principle | How this feature satisfies it |
|---|---|
| I. Specification precedes implementation | research.md answers every question from a pinned header or a pinned binary and records what it cannot answer, with where it lands |
| II. Core portable, shim translation only | every decision in `src/icd/d3d`, driven on Linux by `FakeD3dRuntime` over `MockKmt`; `src/um/d3d` holds the two exports, the root's storage and the record-layout proof, and nothing else |
| III. Object-oriented, with state | every class in the table below owns state; the UMD root is one `ObjectStorage` in the exports' translation unit and the table thunks reach it through `UmdBinding` - the Vulkan ICD's exception taken a third time (PATCH amendment) |
| IV. Test-first and exhaustive | each class over the rig at 100% branches; a hostile table and a fuzz target for the private-data envelope and for the DDI argument records; invariants after every mutation; the allocation-failure sweep over `MockPlatform::fail_allocation_at` |
| V. Everything from outside is hostile | the runtime's structures are memory this driver does not own: a handle it did not hand out, an `Interface` it does not serve, a null callback table, a `DXGI_DDI_BASE_ARGS` arm that does not match the version asked for, and a private-data envelope of the wrong magic, version or length are each refused with the documented result and one log line |
| VI. Feature levels are phases | `GetSupportedVersions` names only the WDDM 3.2 word; `GetCaps` answers only the caps 055 owns; `CreateDevice` answers `E_NOTIMPL` until 056 fills the device functions, and DXCore is therefore told of no D3D11 or D3D12 graphics support |
| VII. IRQL and locking | user mode throughout; the ceiling calls are `BlockSource::require_irql_at_most` as the ICDs' are, and lock order is recorded and asserted (`RecordingThreadLock`, `LockTrace`) |
| VIII. No speculative structure | no class here has one caller in prospect only: the adapter table has two runtimes in it from the first commit, and the DXGI revision selector has the two revisions the two runtimes ask for |
| IX. Reproducible and observable | the WDK headers and `d3d11.dll` are pinned by SHA-256; the UMD's log reaches `diag/` through the same `FileLog` the ICDs use, and `UmdRoot::report` writes the adapter and device counts into a state dump |

**Amendments carried (PATCH):**

1. The list of process-lifetime state gains `rvkumd.dll`'s root behind
   `OpenAdapter10_2` / `OpenAdapter12` and `UmdBinding`'s pointer to it, for
   the reason 045's and 050's entries give: the runtime resolves a bare
   exported name and calls a bare function pointer, and neither is handed a
   context.
2. The DDI-record exemption from the private-members rule widens from
   "`src/core/ddi/`" to "a DDI record directory", gaining
   `src/icd/d3d/ddi/`. These are the same thing: memory the caller owns, WDK
   field names kept so they grep to the header, offsets proven in a shim
   translation unit. `tools/header_scan.py`'s `RECORD_DIRECTORY` becomes a
   tuple, `CLAUDE.md`'s Adaptations section and the constitution's Principle
   III wording say records rather than kernel records.

## Classes this feature introduces

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `UmdRoot` | system (guest UM) | the adapter pool, the device pool, the process connection, the injected seams | Answer the D3D runtimes' adapter-open entry points |
| `UmdBinding` | system | the one pointer to the root | Reach the root from a table thunk handed no context |
| `DdiVersionList` | object | the DDI version words one runtime kind is served at | Answer `GetSupportedVersions` for one runtime |
| `UmdAdapter` | object | one open: the runtime handle, the recorded adapter callbacks, the runtime kind, the chosen DDI revision, its live device count | Be one runtime's open instance of the rvkgpu adapter |
| `AdapterTable` | system | every open adapter of the process | Turn an adapter handle into the adapter that made it |
| `AdapterIdentity` | object | the LUID, the PCI ids, the description, the memory figures | Report the adapter as DXGI and DXCore read it |
| `UmdConnection` | object | the rvkgpu adapter handle, the `GuestConnection`, the `KmtTransport`, its lock | Carry the process's D3D commands to its rvk context |
| `UmdDevice` | object | its adapter, the runtime device handle, the kernel and core-layer callbacks, its DXGI seam, its removal | Be one D3D device the runtime made on this adapter |
| `DeviceTable` | system | the pool of devices and the cookies the runtime holds | Turn a driver device handle into the device that made it |
| `DeviceCookie` | object | the magic and the device it names, in the runtime's own memory | Name one device in memory this driver does not own |
| `DxgiBaseTable` | object | the chosen DXGI revision and the recorded DXGI callbacks | Publish one device's DXGI entry points at the revision the runtime asked for |
| `DeviceRemoval` | object | whether the device is gone, why, and whether that was reported | Say once why a device is gone |
| `UmdPrivateData` | object (proto) | the envelope's magic, version, kind and length | Judge the envelope of anything the UMD hands the kernel |
| `FakeD3dRuntime` | object (harness) | the resolved tables, what each call was given, the refusals a test set | Drive the UMD as `d3d11.dll` and `D3D12Core.dll` do |
| `UmdRig` | object (harness) | the mock platform, kmt, locks, log and root of one process | Stand one UMD process up for a test |

Changed, not new: `src/km/rvkgpu.inf` (the four-entry `UserModeDriverName`,
`rvkumd.dll` in `[Rvkgpu_Files]`), `tools/fetch_windows_kit.sh` (three more
headers), `tools/header_scan.py` (the record directories), `CMakeLists.txt`
(the `av_d3d_umd` archive, the `tests/unit/icd/d3d` link rule, the
`rvkumd_image` gate), `src/um/CMakeLists.txt` (the DLL target).

## Project Structure

### Documentation (this feature)

```text
specs/055-umd-foundation/
├── plan.md, research.md, data-model.md, quickstart.md
├── contracts/umd_interface.md      # INF values, exports, the adapter tables, what 056 inherits
├── contracts/dxgi_seam.md          # DXGI revision selection, the 22 slots, callbacks, what 056/058 fill
├── contracts/private_data.md       # the RVKU envelope the KMD validates
└── tasks.md
```

### Source Code

```text
src/icd/d3d/                      # NEW: the UMD's decisions, portable, in av_d3d_umd
src/icd/d3d/ddi/                  # NEW: mirror records of the WDK structures the DDI passes
src/icd/d3d/umd_entry_points.*    # NEW: the function-table thunks, one line each
src/um/d3d/                       # NEW: rvkumd_exports.cpp, umd_library.*, rvkumd.def, umd_record_layout.cpp
src/proto/resources/umd_private_data.*   # NEW: the RVKU envelope
src/km/rvkgpu.inf                 # UserModeDriverName x4, rvkumd.dll in Files
tools/fetch_windows_kit.sh        # d3d10umddi.h, d3d12umddi.h, dxgiddi.h
tools/header_scan.py              # RECORD_DIRECTORIES
tools/re/binaries.txt             # d3d11.dll
tests/harness/icd/fake_d3d_runtime.*, umd_rig.*
tests/unit/icd/d3d/               # one file per class
tests/loopback/test_fullstack_umd.cpp
tests/fuzz/fuzz_umd_private_data.cpp, fuzz_open_adapter.cpp
```

**Structure Decision**: `src/icd/d3d/` beside `src/icd/vulkan/` and
`src/icd/opengl/`, over `src/icd/common/`. `src/icd/` has been "the guest's
user-mode drivers' decisions" since 045 and a Direct3D driver is one, whatever
the acronym in the directory name says.

## What feature 056 inherits, and where to read it

056 is being built against this spec rather than against this code, so the two
contracts are written for it:

- **`contracts/umd_interface.md`** is the boundary 056 extends: it names every
  adapter-table slot 055 fills, the exact value each answers today, and the
  three that become 056's (`pfnCreateDevice` past the object model,
  `pfnGetCaps` past the identity caps, and the device-functions table the
  create fills). It also states the object model 056 must not re-invent: the
  runtime owns the device's memory, `DeviceCookie` is what lives there, and
  `DeviceTable::device_of` is the only way back to a `UmdDevice`.
- **`contracts/dxgi_seam.md`** names all 22 slots of
  `DXGI1_6_1_DDI_BASE_FUNCTIONS`, which 055 answers and with what, and which
  are left answering `E_NOTIMPL` for 056 (`pfnPresent`, `pfnPresent1`,
  `pfnBlt`, `pfnBlt1`, `pfnRotateResourceIdentities`, `pfnSetDisplayMode`,
  `pfnResolveSharedResource`) and 058 (the two multiplane presents).

Anything 056 needs that is not in those two files is a change to this feature,
not a decision 056 may take alone.

## What feature 061 will need from this

Written down now because nothing in this build can test it: the mirror records
must be size- and offset-checked on x86 as well as x64 (every pointer field
moves every later offset); the table thunks stop being portable function
pointers and become genuinely `__stdcall`, with decorated exports in a second
`.def`; `check_guest_image.py` needs an `i386` machine mode; and the INF gains
`UserModeDriverNameWow` with the same four entries naming the 32-bit build.

## Complexity Tracking

| Violation | Why needed | Simpler alternative rejected because |
|---|---|---|
| A sixth piece of process-lifetime state (the UMD root and `UmdBinding`) | the runtime resolves `OpenAdapter10_2` by name and calls the adapter table's bare function pointers, neither handed a context | one root for all of rvkvk, rvkgl and rvkumd would make each DLL link the other two |
| Mirror records outside `src/core/ddi/` | the UMD DDI hands the driver the runtime's structures exactly as dxgkrnl hands the miniport its own; the layout proof is the same technique | keeping the WDK headers out of the portable core and writing the thunks in `src/um` would put the whole DDI beyond the Linux tests, which is what constitution II exists to prevent |
| A device object in memory the driver did not allocate | `pfnCalcPrivateDeviceSize` / `hDrvDevice` is the ABI; the driver has no say | placement-new into the runtime's block is a third in-place construction site, which `tools/check_design.py` forbids - hence `DeviceCookie`, a plain value in that block naming a device in the driver's own pool |
