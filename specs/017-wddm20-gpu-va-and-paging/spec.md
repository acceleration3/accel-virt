# Feature Specification: WDDM 2.0: GPU virtual addressing, paging queues, monitored fences

**Feature Branch**: `017-wddm20-gpu-va-and-paging`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 017 · **Status**: `implement` · **Depends on**: 016
· **WDDM level**: WDDM 2.0 (Windows 10 1507)

The pivot of the whole project. GPU VA replaces physical addressing and
DxgkDdiPatch is never called again; residency becomes asynchronous; monitored
fences arrive. Everything after this depends on it.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

### DDI slots in scope (13)

| Slot | Field | Introduced |
|--:|---|---|
| 89 | `DxgkDdiRenderGdi` | WDDM2_0 |
| 90 | `DxgkDdiSubmitCommandVirtual` | WDDM2_0 |
| 91 | `DxgkDdiSetRootPageTable` | WDDM2_0 |
| 92 | `DxgkDdiGetRootPageTableSize` | WDDM2_0 |
| 93 | `DxgkDdiMapCpuHostAperture` | WDDM2_0 |
| 94 | `DxgkDdiUnmapCpuHostAperture` | WDDM2_0 |
| 95 | `DxgkDdiCheckMultiPlaneOverlaySupport2` | WDDM2_0 |
| 96 | `DxgkDdiCreateProcess` | WDDM2_0 |
| 97 | `DxgkDdiDestroyProcess` | WDDM2_0 |
| 98 | `DxgkDdiSetVidPnSourceAddressWithMultiPlaneOverlay2` | WDDM2_0 |
| 101 | `DxgkDdiPowerRuntimeSetDeviceHandle` | WDDM2_0 |
| 102 | `DxgkDdiSetStablePowerState` | WDDM2_0 |
| 103 | `DxgkDdiSetVideoProtectedRegion` | WDDM2_0 |

Slot numbers are indices into `DRIVER_INITIALIZATION_DATA` as laid out by the WDDM 3.2 headers; see `docs/ref/driver_init_table.md`. They are the fastest way to navigate an unsymbolized vendor driver.

### Design rules — binding on this feature

Constitution Principle III and [`CLAUDE.md`](../../CLAUDE.md). Restated here because it is checked at review, not assumed:

- **Every unit of behaviour is a class that owns state.** No static member functions. No stateless class used as a namespace. No free function outside the two ABI boundaries (`DxgkDdi*` entry points in `src/core/ddi/`, and `extern "C"` entry points), each one line with no logic.
- If functions thread the same handle, buffer, bounds or platform through their parameters, **that parameter is a member**. Make it a class and inject its dependencies at construction.
- Name the classes this feature adds before writing them, and say which kind each is: an **object class** (one instance per real thing) or a **system class** (a long-lived service owning a collection).
- Dependencies injected at construction. No global, no singleton, no file-scope state.
- Data members private, trailing underscore. Classes `PascalCase`, methods and locals `snake_case`. No `Manager`, `Helper`, `Utils`, `Handler`, `Impl`, `process()`, `handle()`.
- One class per header, named for it. A header declares; it does not define. One responsibility per class, statable without an "and".
- Errors are `av::Status` values. No exceptions, no RTTI, no C++ standard library, no `new`/`delete` (placement new only in `ObjectStorage`). Cleanup is RAII via `ScopeExit`, never repeated per branch.
- Fallible construction is two-phase: trivial constructor, `initialize(...)` returning `Status`.
- Every public method declares its IRQL ceiling via `platform_.require_irql_at_most(...)`.

### Classes this feature introduces

<!-- Fill this in during /speckit-plan, before writing code. One row per class; if a row has no state, it is not a class. -->

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `GpuMmuGeometry` | object | platform, virtual address bits, level count, index bits per level, PTE size | Describes the GPU page tables VidMm builds for this adapter |
| `PageTableEntry` | object | one `DXGK_PTE` | Encodes one VidMm PTE in the device's page table format |
| `PageTableUpdate` | object | platform, geometry, one `DXGK_BUILDPAGINGBUFFER_UPDATEPAGETABLE` | Writes one UPDATE_PAGE_TABLE into a CPU-mapped page table |
| `PagingCommand` (proto) | object | operation, addresses, sizes, pattern, ranges count | The wire form of one GPU-VA paging operation in a DMA buffer |
| `PagingRequest` (extended) | object | + the WDDM 2.0 operations | Answers one DxgkDdiBuildPagingBuffer |
| `RootPageTable` | object | segment id, segment offset, entries | The root page table one context runs on |
| `Context` (extended) | object | + its `RootPageTable` | One scheduling context |
| `DriverCapabilities` (extended) | object | + physical adapter caps, WDDM 2.0 claims | Reports what the driver implements |
| `DisplaySources` (extended) | object | scanout addresses stored atomically instead of under a spin lock | Remembers each source's display state |
| `Platform` (extended) | system contract | + atomic 64-bit store and load | What core may ask of the kernel |
| DDI records | DDI record | `DXGK_GPUMMUCAPS`, `DXGK_PAGE_TABLE_LEVEL_DESC`, `DXGK_PHYSICALADAPTERCAPS`, `DXGK_PTE`, the WDDM 2.0 paging operations, `DXGKARG_SUBMITCOMMANDVIRTUAL`, `DXGKARG_SETROOTPAGETABLE`, `DXGKARG_GETROOTPAGETABLESIZE`, `DXGKARG_CREATEPROCESS` | Overlays memory dxgkrnl owns, layout-proven |
| `FakeGpuMmu` (harness) | object | the caps and level descriptors VidMm read | `ADAPTER_RENDER::Initialize` and `InitializeGpuVaState`'s GPU MMU checks |

### Test requirements

Constitution Principle III applies in full. Concretely, this feature is not done until:

- every branch of every function it adds to `src/core`, `src/device` or `src/proto` is covered, or marked `AV_UNTESTABLE` with a justification;
- each stateful module it adds exports an invariant function that tests call after **every** mutation;
- every function it adds that reads bytes from outside its own address space has a hostile-input table test and a libFuzzer target with a committed corpus;
- the allocation-failure sweep passes: with each allocation site failed in turn, no crash and no leak;
- IRQL annotations are present and the mock's IRQL checker is satisfied;
- the DDI conformance harness (feature 003) drives every slot above, including the abusive-but-legal sequences for it;
- `tools/check_symbol_tests.py` reports no untested exported symbol.

### Reference material

Knowledge base:

- [`docs/kb/04-memory.md`](../../docs/kb/04-memory.md)
- [`docs/kb/03-object-model.md`](../../docs/kb/03-object-model.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11

- Q (agent, RE): What does dxgkrnl require of a WDDM 2.0 adapter? -> A:
  `DXGADAPTER::Initialize` @1401a2818 asks `PHYSICALADAPTERCAPS` (15) per
  physical adapter when WDDMVersion >= 2.0; answered, `SchedulingCaps.MultiEngineAware`
  is mandatory (`"SchedulingCaps.MultiEngineAware is not set by WDDMv2 driver"`).
  `CreateRenderCore` @1401b13d8 requires `FlipCaps.FlipOnVSyncMmIo` from 2.0
  unless ComputeOnly (`"Driver reports WDDM version 2.0 or higher but does not
  support FlipOnVSyncMmIo cap"`, `STATUS_INVALID_PARAMETER`); CreateAllocation,
  Patch, Render and QueryCurrentFence stop being required. The 1.2/1.3
  feature checks and the full-adapter DirectFlip/FlipIndependent checks run only
  with the `GraphicsDrivers\ValidateWDDMCaps` registry value (`DXGADAPTER+0xBE0`),
  off by default, so this adapter does not claim DirectFlip or FlipIndependent;
  docs/ddi-sequences.md and `FakeDxgadapter` are corrected.
- Q (agent, RE): Which GPU virtual addressing model? -> A: GPU MMU.
  `basicrender.sys` uses the IoMmu model (the GPU shares the CPU process page
  tables), which would have the device walk guest process page tables. With GPU
  MMU VidMm owns the GPU page tables in guest system memory and the driver only
  encodes their entries; the device walks them by guest physical address.
  `ADAPTER_RENDER::Initialize` @1401b3d4c then asks `GPUMMUCAPS` (13) and
  `PAGETABLELEVELDESC` (14) per level; VidMm's `InitializeGpuVaState` @1400972cc
  validates them and adds an implicit system memory segment (id NbSegment+1).
  Both `VirtualAddressingSupported` (VidMm keys on it) and `GpuMmuSupported`
  (dxgkrnl keys on it) are set, in DRIVERCAPS and in PHYSICALADAPTERCAPS, and the
  node's metadata says `GpuMmuSupported`.
- Q (agent): The geometry? -> A: x86-64's: 48 virtual address bits, four
  levels of 512 8-byte entries (4096-byte tables, 9 index bits each; level 0 is
  the leaf), tables in segment 0 (system memory) for processes and the paging
  process, `DXGK_PAGETABLEUPDATE_CPU_VIRTUAL` so entries are written through the
  CPU mapping at BuildPagingBuffer time, cache-coherent memory supported. No
  64K or large pages, no read-only, no-execute or zeroed PTE claims.
- Q (agent): The device's page table entry? -> A: 8 bytes, little-endian: bit 0
  valid, bit 1 read-only, bit 2 no-execute, bit 3 cache-coherent, bits 4-8 the
  segment, bits 12-63 the page frame (`DXGK_PTE.PageAddress`, which is already
  the high 52 bits). Documented in docs/protocol.md.
- Q (agent, RE): Which paging operations? -> A: WDDM 2.0 VidMm issues the GPU-VA
  forms (TRANSFER, FILL and DISCARD_CONTENT are "WDDMv1 Only"). UPDATE_PAGE_TABLE
  writes entries through the CPU mapping (`Repeat` writes the first entry to all);
  FLUSH_TLB, VIRTUAL_FILL, VIRTUAL_TRANSFER and COPY_PAGE_TABLE_ENTRIES (whose
  page tables are named by GPU virtual address in the paging process) become
  `PagingCommand`s in the DMA buffer for the device (multipass over copy ranges);
  NOTIFY_RESIDENCY, MAP_APERTURE_SEGMENT and UNMAP_APERTURE_SEGMENT need nothing.
  Context-allocation operations are refused: the driver requests none. Which
  operations VidMm actually issues in which order was not determined.
- Q (agent, RE): Root page tables and submission? -> A: `SetRootPageTable`,
  `GetRootPageTableSize` and `SubmitCommandVirtual` are called through without a
  NULL check once a node supports GPU VA, so all three are registered.
  `SetRootPageTable` records the root on the context; `GetRootPageTableSize`
  answers NumberOfPte x 8. `SubmitCommandVirtual` (PASSIVE) queues the fence on
  the node like SubmitCommand. `CreateProcess` answers no driver process (as
  basicrender); `DestroyProcess` succeeds.
- Q (agent): Allocations? -> A: Supported and preferred in the implicit system
  memory segment (id 2); the aperture segment stays, holding paging buffers.
- Q (agent, RE): FlipOnVSyncMmIo? -> A: dxgkrnl then calls
  `SetVidPnSourceAddress` synchronized with the interrupt, at device IRQL, so the
  scanout address is stored and loaded atomically (`InterlockedExchange64`)
  instead of under a spin lock.
- Q (agent): The rest of the slots? -> A: `SetStablePowerState` succeeds (as
  basicrender; dxgkrnl reads it at DDI > 0x5007). Skipped, NULL: `RenderGdi`
  (no GDI acceleration), `MapCpuHostAperture`/`UnmapCpuHostAperture` (no CPU host
  aperture segment), `CheckMultiPlaneOverlaySupport2` and
  `SetVidPnSourceAddressWithMultiPlaneOverlay2` (no overlays),
  `PowerRuntimeSetDeviceHandle` (no runtime power), `SetVideoProtectedRegion`
  (no protected region).

## User Scenarios & Testing *(mandatory)*

Users: dxgkrnl's adapter initialization and render core, VidMm's GPU VA state,
paging engine and residency, VidSch's virtual submission and flips.

### User Story 1 - Starting as a WDDM 2.0 GPU MMU adapter (Priority: P1)

1. **Given** start, **Then** DRIVERCAPS reports 2.0, MultiEngineAware, one node,
   VirtualAddressing and GpuMmu, FlipOnVSyncMmIo; PHYSICALADAPTERCAPS one node,
   paging node 0, GpuMmu; GPUMMUCAPS 48 bits, 4 levels, CPU-virtual updates;
   each PAGETABLELEVELDESC 9 bits, segment 0, 4096 bytes; node metadata GpuMmu.
   The fake dxgkrnl and VidMm accept all of it and create the render core with
   two segments.
2. A level index past 3 or a physical adapter other than 0 is
   `STATUS_INVALID_PARAMETER`; short buffers likewise.

### User Story 2 - Page tables (Priority: P1)

1. **Given** UPDATE_PAGE_TABLE of three entries at index 7 of a table, **Then**
   bytes 56..79 hold the three device PTEs; with `Repeat`, all three equal the
   first; an index range past 512, a null table or null entries is refused and
   nothing is written.
2. FLUSH_TLB, VIRTUAL_FILL and VIRTUAL_TRANSFER each write one `PagingCommand`;
   COPY_PAGE_TABLE_ENTRIES writes as many ranges as fit and answers
   `STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER` with MultipassOffset for the rest.

### User Story 3 - Virtual submission (Priority: P1)

1. CreateContext with `VirtualAddressing`; SetRootPageTable records the root;
   SubmitCommandVirtual queues the fence and the DPC completes it;
   GetRootPageTableSize answers 4096 for 512 entries.
2. An unknown context is refused (logged for SetRootPageTable, which returns
   nothing).

### User Story 4 - Flips at device IRQL (Priority: P1)

1. SetVidPnSourceAddress at device IRQL stores the address the next vsync reports.

## Requirements *(mandatory)*

- **FR-001**: Every field of a paging operation is checked before a byte is
  written; nothing is written outside the page table or DMA buffer (fuzz).
- **FR-002**: Capabilities reported are exactly those implemented here and in
  features 011-016.

## Success Criteria *(mandatory)*

- **SC-001**: Through the table: start with every WDDM 2.0 query, a context with
  a root page table, page table updates, a virtual submission completing, a flip
  at device IRQL reported at vsync - no leak, no violation.
- **SC-002**: 100% branch coverage; fuzz_paging clean.

## Out of Scope

- Executing paging commands and walking page tables on the device (030-032),
  IoMmu, large pages, DirectFlip and independent flip, hardware queues (019).
