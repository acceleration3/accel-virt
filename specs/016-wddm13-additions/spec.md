# Feature Specification: WDDM 1.3 additions

**Feature Branch**: `016-wddm13-additions`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 016 · **Status**: `partial` · **Depends on**: 015
· **WDDM level**: WDDM 1.3 (Windows 8.1)

### Status meaning

Implement only the subset listed below. Everything else in this level is stubbed or skipped, and the stub is tested.

### DDI slots in scope (6)

| Slot | Field | Introduced |
|--:|---|---|
| 83 | `DxgkDdiGetNodeMetadata` | WDDM1_3 |
| 84 | `DxgkDdiSetPowerPState` | WDDM1_3 |
| 85 | `DxgkDdiControlInterrupt2` | WDDM1_3 |
| 86 | `DxgkDdiCheckMultiPlaneOverlaySupport` | WDDM1_3 |
| 87 | `DxgkDdiCalibrateGpuClock` | WDDM1_3 |
| 88 | `DxgkDdiFormatHistoryBuffer` | WDDM1_3 |

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
| `Platform` (extended) | system contract | + the performance counter | What core may ask of the kernel |
| DDI records | DDI record | `DXGK_NODEMETADATA`, `DXGK_GPUCLOCKDATA`, `DXGKARG_CONTROLINTERRUPT2` | Overlays memory dxgkrnl owns, layout-proven |

No new object class: the adapter answers each DDI from state it already owns.

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

- Q (agent): Which of the six WDDM 1.3 slots? -> A: Implemented:
  `GetNodeMetadata`, `ControlInterrupt2`, `CalibrateGpuClock`. Stubbed:
  `SetPowerPState` (no power component is reported, so every index is
  `STATUS_INVALID_PARAMETER`). Skipped, NULL: `CheckMultiPlaneOverlaySupport`
  (overlays are declined; `DXGADAPTER::Initialize` only looks for it with
  `SupportMultiPlaneOverlay`) and `FormatHistoryBuffer` (only used with history
  buffers the driver does not report).
- Q (agent, RE): Node metadata? -> A: As `basicrender.sys`
  `WarpKMGetNodeMetadata`: zero the structure, `EngineType` 3D, friendly name
  `3DNode%02X` of the node ordinal (the low word; the high word is the physical
  adapter index). Only node 0 of adapter 0.
- Q (agent, RE): GPU clock? -> A: As `WarpKMCalibrateGpuClock`: the GPU clock is
  the CPU performance counter, so `GpuClockCounter` and `CpuClockCounter` are one
  `KeQueryPerformanceCounter` reading and `GpuFrequency` its frequency. The
  platform gains `performance_counter`. Node 0, engine 0 only.
- Q (agent): ControlInterrupt2? -> A: The same synthetic vsync as
  ControlInterrupt: `DXGK_VSYNC_ENABLE` starts it, both disable states stop it (a
  timer has no phase to keep); another state is `STATUS_INVALID_PARAMETER`,
  another interrupt type `STATUS_NOT_SUPPORTED`.

## User Scenarios & Testing *(mandatory)*

Users: dxgkrnl's scheduler statistics and GPUView (node metadata, clock
calibration) and the vsync control path of WDDM 1.3.

### User Story 1 (Priority: P1)

1. GetNodeMetadata for node 0 answers a 3D engine named `3DNode00`; node 1 or
   adapter 1 is `STATUS_INVALID_PARAMETER`.
2. ControlInterrupt2 enable then disable-keep-phase starts and stops vsync as
   ControlInterrupt does.
3. CalibrateGpuClock answers equal GPU and CPU counters and the frequency.
4. SetPowerPState refuses every component.

## Requirements *(mandatory)*

- **FR-001**: No capability changes.

## Success Criteria *(mandatory)*

- **SC-001**: Through the table, metadata, clock and vsync control behave as
  above; the skipped slots are NULL and registration still succeeds.
- **SC-002**: 100% branch coverage.

## Out of Scope

- Multi-plane overlay, history buffers, power P-states.
