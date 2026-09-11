# Feature Specification: WDDM 1.0: present and scanout

**Feature Branch**: `012-wddm10-present-and-scanout`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 012 · **Status**: `implement` · **Depends on**: 011, 008
· **WDDM level**: WDDM 1.0 (Windows Vista)

Getting pixels on screen, and the synthetic vsync DWM's frame pacing depends
on.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

### DDI slots in scope (3)

| Slot | Field | Introduced |
|--:|---|---|
| 40 | `DxgkDdiSetVidPnSourceAddress` | BASE |
| 41 | `DxgkDdiSetVidPnSourceVisibility` | BASE |
| 54 | `DxgkDdiPresent` | BASE |

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
| `PeriodicTimer` | object | platform, callback, context, the platform timer | Owns one periodic kernel timer from creation to deletion |
| `SyntheticVsync` | object | platform, dxgkrnl, display sources, its own interrupt notification, a `PeriodicTimer` | Raises CRTC_VSYNC for target 0 at the refresh rate while dxgkrnl wants it |
| `PresentRequest` | object | platform, one `DXGKARG_PRESENT` | Encodes one present into its DMA buffer and patch list |
| `PresentCommand` (proto) | object | flags, colour, rectangles, sub-rectangle count | The wire form of one present in a DMA buffer |
| `SourceScanout` | object | platform, spin lock, primary allocation, segment, address, visibility | Remembers what one source scans out, readable at DISPATCH |
| `DisplaySources` (extended) | object | + one `SourceScanout` per source | Remembers each source's display state |
| `InterruptNotification` (extended) | object | + CRTC_VSYNC data | Reports one interrupt to dxgkrnl from outside the ISR |
| DDI records | DDI record | `DXGKARG_PRESENT`, `DXGKARG_SETVIDPNSOURCEADDRESS`, `DXGKARG_SETVIDPNSOURCEVISIBILITY` | Overlays memory dxgkrnl owns, layout-proven |

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

- [`docs/kb/06-display.md`](../../docs/kb/06-display.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11

- Q (agent, RE): What does a minimal Present do? -> A: `basicrender.sys`
  `WARPKMCONTEXT::Present` builds one command object in the DMA buffer holding
  the flags, colour, source and destination rectangles and the sub-rectangles,
  writes patch locations for allocation-list entries 1 (source) and 2
  (destination) when the output patch list is given, advances `pDmaBuffer` and
  `pPatchLocationListOut` past what it wrote, and answers `STATUS_NO_MEMORY`
  only when it cannot hold the rectangles. This driver has no pool at hand for
  them, so a DMA buffer too small for every sub-rectangle is filled with as
  many as fit and answers `STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER`
  (0xC01E0001) with `MultipassOffset` naming the next one, which dxgkrnl
  resubmits.
- Q (agent): What executes a present? -> A: The device (features 027 and 030).
  Until then the loopback engine completes the packet like any other. The
  command's wire form is part of the protocol (docs/protocol.md) so the device
  can execute it later: magic "RVKP", little-endian, 72 bytes then 16 per
  sub-rectangle, the source and destination addresses patched in place.
- Q (agent): Where does vsync come from? -> A: A platform timer at the committed
  mode's refresh rate (60 Hz), created at StartDevice, running only between
  `ControlInterrupt(CRTC_VSYNC, TRUE)` and `FALSE` (or StopDevice). Each tick
  reports `DXGK_INTERRUPT_CRTC_VSYNC` for target 0 with the address last set by
  `SetVidPnSourceAddress`, through `DxgkCbSynchronizeExecution`, then
  `DxgkCbNotifyDpc`. The platform contract gains a periodic timer
  (`ExAllocateTimer` with `EX_TIMER_HIGH_RESOLUTION`).
- Q (agent): At what IRQL is SetVidPnSourceAddress called? -> A: The header
  allows up to device IRQL, which dxgkrnl uses only for a driver reporting
  `FlipOnVSyncMmIo`. This driver reports no flip capability, so it accepts
  DISPATCH and below and guards the scanout with a spin lock the vsync tick
  also takes.
- Q (agent): Capabilities? -> A: None change: no flip caps, no presentation
  caps. The render core rule now passes: dxgkrnl creates the render core.

## User Scenarios & Testing *(mandatory)*

Users: dxgkrnl's presentation path (DWM on WARP presenting to the primary),
VidSch's flip path, and DWM's frame pacing, which waits on vsync.

### User Story 1 - Present (Priority: P1)

1. **Given** Present with `Blt` from allocation 1 to 2 and three
   sub-rectangles, a DMA buffer large enough, **Then** one command with both
   rectangles and the three sub-rectangles, patch locations for the source and
   destination addresses, `pDmaBuffer` and `pPatchLocationListOut` advanced.
2. **Given** `ColorFill`, **Then** the colour and only the destination patch.
3. **Given** `Flip`, **Then** only the source patch.
4. **Given** a DMA buffer with room for one sub-rectangle of three, **Then**
   `STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER`, `MultipassOffset` 1; called again
   with it, the rest.
5. **Given** no operation flag, two operations, or an unsupported flag
   (colour keys, rotation, linear-to-sRGB, stereo, overlay), a null DMA buffer,
   a null rectangle list with a non-zero count, or a patch list without room,
   **Then** `STATUS_INVALID_PARAMETER`, nothing written.

### User Story 2 - Scanout and vsync (Priority: P1)

1. **Given** SetVidPnSourceAddress for source 0 with a primary in segment 1,
   **Then** the address is remembered; another source, or a primary in another
   segment, is `STATUS_INVALID_PARAMETER`; a null allocation clears it.
2. **Given** SetVidPnSourceVisibility, **Then** the visibility is remembered.
3. **Given** vsync enabled, **When** a frame period passes, **Then** one
   CRTC_VSYNC notification for target 0 carrying the scanout address, then a
   DPC notification; disabled, none. ControlInterrupt before StartDevice is
   `STATUS_INVALID_DEVICE_STATE`.
4. **Given** StopDevice with vsync enabled, **Then** the timer is gone and no
   tick follows.

## Requirements *(mandatory)*

- **FR-001**: Every field read from `DXGKARG_PRESENT` and its rectangles is
  checked before a byte is written; nothing is written outside the DMA buffer
  or the output patch list (fuzz).
- **FR-002**: The vsync tick allocates nothing and takes only the scanout spin
  lock; it never shares notification data with the DPC.
- **FR-003**: No capability changes.

## Success Criteria *(mandatory)*

- **SC-001**: Through the table: mode set, primary address, vsync on, ticks
  reported with the address, present blt, submission completes, vsync off,
  stop - no leak, no violation.
- **SC-002**: 100% branch coverage; fuzz_present clean.

## Out of Scope

- Executing presents and scanning out on the device (027, 030); flip
  capabilities, overlays, periodic frame notification (WDDM 2.2).
