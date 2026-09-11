# Feature Specification: WDDM 1.0: contexts and packet submission

**Feature Branch**: `009-wddm10-context-and-submission`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 009 · **Status**: `partial` · **Depends on**: 008
· **WDDM level**: WDDM 1.0 (Windows Vista)

The first vertical slice of execution: record, patch, submit, complete. Uses
physical addressing and DxgkDdiPatch, which WDDM 2.0 replaces with GPU virtual
addressing. Deliberately throwaway: it proves the Render → Submit → fence
pipeline with the least machinery.

### Status meaning

Implement only the subset listed below. Everything else in this level is stubbed or skipped, and the stub is tested.

### DDI slots in scope (8)

| Slot | Field | Introduced |
|--:|---|---|
| 25 | `DxgkDdiPatch` | BASE |
| 26 | `DxgkDdiSubmitCommand` | BASE |
| 27 | `DxgkDdiPreemptCommand` | BASE |
| 28 | `DxgkDdiBuildPagingBuffer` | BASE |
| 36 | `DxgkDdiQueryCurrentFence` | BASE |
| 53 | `DxgkDdiRender` | BASE |
| 58 | `DxgkDdiCreateContext` | BASE |
| 59 | `DxgkDdiDestroyContext` | BASE |

Slot numbers are indices into `DRIVER_INITIALIZATION_DATA` as laid out by the WDDM 3.2 headers; see `docs/ref/driver_init_table.md`. They are the fastest way to navigate an unsymbolized vendor driver.

### Note

Patch and physical addressing are retired by feature 017. Keep the
implementation small and do not build on it.

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
| `Context` | object | platform, device, node ordinal, creation flags | One execution context on a device |
| `ContextTable` | system | platform, mutex, context pool | Owns the adapter's contexts |
| `RenderRequest` | object | platform, one `DXGKARG_RENDER` | Moves one command buffer into its DMA buffer |
| `PatchRequest` | object | platform, one `DXGKARG_PATCH` | Writes one submission's allocation addresses |
| `PagingRequest` | object | platform, one `DXGKARG_BUILDPAGINGBUFFER` | Answers one paging operation |
| `ExecutionNode` | object | platform, spin lock, pending fence ring, last submitted, last completed, preemption request | Tracks one node's packets from submission to completion |
| `InterruptNotification` | object | dxgkrnl interface, the notify-interrupt data | Reports one completion to dxgkrnl at device IRQL |
| DDI records | DDI record | the fields of one WDK structure | Overlays memory dxgkrnl owns, layout-proven |
| `FakeVidSch` (harness) | object | platform, table, adapter, submitted and reported fences | Drives render, patch and submission the way VidSch does |

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

- [`docs/kb/05-scheduling.md`](../../docs/kb/05-scheduling.md)
- [`docs/kb/10-warp-kmd.md`](../../docs/kb/10-warp-kmd.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11

- Q (agent): There is no executor yet - the device side arrives with
  030-032. What runs a submitted packet? -> A: A loopback engine: the DPC the
  submission queues completes every pending packet in fence order and reports
  `DXGK_INTERRUPT_DMA_COMPLETED` through `DxgkCbSynchronizeExecution` then
  `DxgkCbNotifyDpc`, the recipe `basicrender.sys` uses for completions without
  an interrupt (`WARPKMADAPTER::WarpSynchronizeNotifyInterrupt`). Completion
  never happens inside SubmitCommand, which runs while VidSch is submitting.
  Feature 030 replaces the engine with the device ring; this feature proves the
  record -> patch -> submit -> fence plumbing.
- Q (agent, RE): Which paging operations does an aperture-only adapter see? ->
  A: VidMm's `TransferToSegment` (memory segments) is what issues FILL and
  TRANSFER; an aperture segment is populated with `MapVideoApertureSegment` /
  `UnmapVideoApertureSegmentInternal` (dxgmms2), and discards with
  `DiscardAllocation`. So this feature answers `MAP_APERTURE_SEGMENT`,
  `UNMAP_APERTURE_SEGMENT` and `DISCARD_CONTENT` - with nothing written, since
  the aperture is the allocation's own guest pages - and refuses every other
  operation with `STATUS_INVALID_PARAMETER` (the `partial` status).
- Q (agent): Node topology? -> A: One node, one engine, no `MultiEngineAware`:
  dxgkrnl then passes `hDevice` to Patch and SubmitCommand and sets one node
  itself (`CreateRenderCore`). No capability changes.
- Q (agent): DMA buffer layout? -> A: Opaque to the driver: the command buffer
  is copied verbatim and each patch location receives the 64-bit little-endian
  segment address plus allocation offset. Context info: 64 KiB DMA buffers in
  system memory, no private data, 256 allocations, 1024 patch locations.

## User Scenarios & Testing *(mandatory)*

Users: VidSch, which submits packets and waits for fences; the render core
rule, which now needs only TDR, ControlInterrupt and Present.

### User Story 1 - Contexts (Priority: P1)

1. **Given** CreateContext on a device for node 0, **Then** a context handle
   and the context info above; another node is `STATUS_INVALID_PARAMETER`; a
   context on a device that is not ours is `STATUS_INVALID_PARAMETER`.
2. **Given** DestroyContext, **Then** the context is released; allocation
   failure during CreateContext leaves nothing.

### User Story 2 - Render and Patch (Priority: P1)

1. **Given** a command buffer that fits the DMA buffer, **Then** Render copies
   it, copies each patch location after checking its allocation index and that
   eight bytes at its offset lie inside the command, and advances the DMA and
   patch-list-out pointers.
2. **Given** a command longer than the DMA buffer, a patch location outside
   the command, an allocation index outside the list, or an output patch list
   too short, **Then** `STATUS_INVALID_PARAMETER` and nothing advanced.
3. **Given** Patch for a submission range, **Then** each location in the range
   receives its allocation's address plus offset; any location, range or offset
   outside the buffer refuses the whole patch before writing.

### User Story 3 - Submission and fences (Priority: P1)

1. **Given** SubmitCommand at DISPATCH with a fence above the last submitted,
   **Then** it is queued and a DPC requested; nothing allocates or blocks.
2. **Given** the DPC, **Then** every pending fence is reported completed in
   order under `DxgkCbSynchronizeExecution`, then `DxgkCbNotifyDpc`.
3. **Given** QueryCurrentFence, **Then** the last completed fence.
4. **Given** PreemptCommand, **Then** the next DPC reports
   `DXGK_INTERRUPT_DMA_PREEMPTED` with the preemption fence and the last
   completed fence, after completing what was pending.
5. **Given** a fence not above the last submitted, a full queue, or another
   node, **Then** `STATUS_INVALID_PARAMETER` and nothing queued.

### User Story 4 - Paging (Priority: P2)

1. **Given** BuildPagingBuffer for MAP_APERTURE_SEGMENT, UNMAP_APERTURE_SEGMENT
   or DISCARD_CONTENT, **Then** success with no DMA bytes written.
2. **Given** any other operation, **Then** `STATUS_INVALID_PARAMETER`.

### Edge Cases

- A patch offset within eight bytes of the submission end.
- `PatchLocationListSubmissionStart + Length` overflowing 32 bits.
- Preempt with nothing pending; two preempts before a DPC.
- A DPC with nothing pending reports nothing but still notifies the DPC.

## Requirements *(mandatory)*

- **FR-001**: SubmitCommand, PreemptCommand and the DPC MUST not allocate,
  block or take a mutex; the node state is under a spin lock.
- **FR-002**: Every size, offset and index in Render and Patch MUST be checked,
  with overflow-safe arithmetic, before any byte is read or written; both are
  fuzzed.
- **FR-003**: Completions MUST be reported only from the DPC, in fence order,
  each under `DxgkCbSynchronizeExecution`.
- **FR-004**: The harness MUST model VidSch's side: fences monotonic, every
  completion for a submitted fence, the current fence matching.

## Success Criteria *(mandatory)*

- **SC-001**: A scenario creates a context, renders, patches, submits three
  packets, runs the DPC and sees three completions and the current fence, with
  no violation.
- **SC-002**: 100% branch coverage; the submission fuzz target is clean.
- **SC-003**: The render core rule's first missing slot moves to
  `ResetFromTimeout`.

## Assumptions

- VidSch never submits more packets than the queue holds (64) before a DPC.

## Out of Scope

- Real execution (030-032), GPU virtual addressing and SubmitCommandVirtual
  (017), TDR (010), multiple nodes.
