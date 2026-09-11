# Feature Specification: WDDM 2.4 additions

**Feature Branch**: `021-wddm24-additions`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 021 · **Status**: `partial` · **Depends on**: 019
· **WDDM level**: WDDM 2.4 (1803)

### Status meaning

Implement only the subset listed below. Everything else in this level is stubbed or skipped, and the stub is tested.

### DDI slots in scope (12)

| Slot | Field | Introduced |
|--:|---|---|
| 132 | `DxgkDdiSetSchedulingLogBuffer` | WDDM2_4 |
| 133 | `DxgkDdiSetupPriorityBands` | WDDM2_4 |
| 134 | `DxgkDdiNotifyFocusPresent` | WDDM2_4 |
| 135 | `DxgkDdiSetContextSchedulingProperties` | WDDM2_4 |
| 136 | `DxgkDdiSuspendContext` | WDDM2_4 |
| 137 | `DxgkDdiResumeContext` | WDDM2_4 |
| 138 | `DxgkDdiSetVirtualMachineData` | WDDM2_4 |
| 139 | `DxgkDdiBeginExclusiveAccess` | WDDM2_4 |
| 140 | `DxgkDdiEndExclusiveAccess` | WDDM2_4 |
| 141 | `DxgkDdiQueryDiagnosticTypesSupport` | WDDM2_4 |
| 142 | `DxgkDdiControlDiagnosticReporting` | WDDM2_4 |
| 143 | `DxgkDdiResumeHwEngine` | WDDM2_4 |

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
| `DriverCapabilities` (extended) | object | WDDM 2.4 | Reports what the driver implements |
| DDI record | DDI record | `DXGKARG_RESUMEHWENGINE` | Overlays memory dxgkrnl owns, layout-proven |

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

- [`docs/kb/08-wddm3x-delta.md`](../../docs/kb/08-wddm3x-delta.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11

- Q (agent, RE): What does WDDM 2.4 change for this driver? -> A:
  `InitializeDriverWorkarounds @ 1401a4fc8` forces `FlipCaps.FlipImmediateMmIo`
  on for every non-IndirectKmd adapter, so an immediate present reaches
  SetVidPnSourceAddress with `FlipImmediate` (0x2), at device IRQL like a vsync
  flip (`VidSchiExecuteMmIoFlip @ 14000f440`). VidSch completes it right after the
  call and expects no report; the address shows from the next scanout, which the
  driver's scanout already does. `FlipImmediateNoTearing` never arrives through
  this DDI. `MiscCaps` bits 9 and up and the diagnostic pair are checked, both
  satisfied.
- Q (agent, RE): Which of the twelve slots? -> A: Implemented:
  `ResumeHwEngine` (143), which `VidSchiResetHwEngine @ 140053e38` calls without a
  NULL check after a successful `ResetHwEngine`: node 0, engine 0. Left NULL:
  `SetSchedulingLogBuffer`, `SetupPriorityBands`, `SetContextSchedulingProperties`,
  `ResumeContext` (NOT_IMPLEMENTED accepted), `SuspendContext` (NULL is a
  synchronous suspend), `NotifyFocusPresent` (hardware queue paths only),
  `SetVirtualMachineData` (GPU-PV host only), `Begin/EndExclusiveAccess`
  (IoMmuSecureMode only), and the diagnostic pair (both NULL installs dxgkrnl's
  defaults; one alone fails start).
- Q (agent, RE): Enable hardware scheduling now? -> A: No. It needs WDDM 2.5,
  the feature query at StartDevice, `SignalMonitoredFence`, context
  invalidation inside ResetHwEngine, HwQueueSupported contexts throughout the
  node, paging through a paging hardware queue - and a user-mode driver, since
  D3DKMTRender is refused on such a node. It gets its own feature once a user
  mode driver exists.
- Q (agent): WDDMVersion? -> A: 2.4.

## User Scenarios & Testing *(mandatory)*

Users: dxgkrnl's adapter start and VidSch's immediate flips and engine
recovery.

### User Story 1 - Starting a WDDM 2.4 adapter (Priority: P1)

1. **Given** start, **Then** DRIVERCAPS reports 2.4 and every check passes; the
   eleven skipped slots are NULL.

### User Story 2 - Immediate flips (Priority: P1)

1. **Given** a visible source with vsync on, **When** VidSch flips immediately at
   device IRQL, **Then** no report is made for the flip and the next vsync shows
   the new primary.

### User Story 3 - Engine recovery (Priority: P1)

1. ResetHwEngine then ResumeHwEngine for node 0 engine 0 succeed; another node
   or engine is refused.

## Requirements *(mandatory)*

- **FR-001**: The only capability change is WDDMVersion 2.4.

## Success Criteria *(mandatory)*

- **SC-001**: Through the table: start at 2.4, an immediate flip shown at the
  next vsync, an engine reset and resume, no violation.
- **SC-002**: 100% branch coverage.

## Out of Scope

- Hardware scheduling, scheduling logs and priority bands, context suspension,
  diagnostics, exclusive access, virtual machine data.
