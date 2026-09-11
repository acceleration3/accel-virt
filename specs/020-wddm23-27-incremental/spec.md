# Feature Specification: WDDM 2.3 – 2.7 incremental additions

**Feature Branch**: `020-wddm23-27-incremental`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 020 · **Status**: `partial` · **Depends on**: 019
· **WDDM level**: WDDM2_3+

Four sparse levels taken together. Each is a handful of slots and none
restructures anything.

### Status meaning

Implement only the subset listed below. Everything else in this level is stubbed or skipped, and the stub is tested.

### DDI slots in scope (12)

| Slot | Field | Introduced |
|--:|---|---|
| 129 | `DxgkDdiUpdateHwContextState` | WDDM2_3 |
| 130 | `DxgkDdiCreateProtectedSession` | WDDM2_3 |
| 131 | `DxgkDdiDestroyProtectedSession` | WDDM2_3 |
| 144 | `DxgkDdiSignalMonitoredFence` | WDDM2_5 |
| 145 | `DxgkDdiPresentToHwQueue` | WDDM2_5 |
| 146 | `DxgkDdiValidateSubmitCommand` | WDDM2_5 |
| 147 | `DxgkDdiSetTargetAdjustedColorimetry2` | WDDM2_5 |
| 148 | `DxgkDdiSetTrackedWorkloadPowerLevel` | WDDM2_5 |
| 149 | `DxgkDdiSaveMemoryForHotUpdate` | WDDM2_6 |
| 150 | `DxgkDdiRestoreMemoryForHotUpdate` | WDDM2_6 |
| 151 | `DxgkDdiCollectDiagnosticInfo` | WDDM2_6 |
| 153 | `DxgkDdiControlInterrupt3` | WDDM2_7 |

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
| `DriverCapabilities` (extended) | object | WDDM 2.3 | Reports what the driver implements |

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

- Q (agent, RE): What do WDDM 2.3, 2.5, 2.6 and 2.7 require? -> A: For this
  capability set, no DDI, capability or fatal query at any step to 2.9. 2.3:
  colour transform checks (none reported); `HwQueuePacketCap` is no longer
  cleared; disabling a primary flushes present references instead of the
  legacy synchronization (`IsLegacyDisplayStateSynchronization @ 14005829c`).
  2.4: `MiscCaps` reserved bits, the diagnostic DDI pair, and
  `InitializeDriverWorkarounds @ 1401a4fc8` forces `FlipCaps.FlipImmediateMmIo`
  on - so reporting 2.4 waits for SetVidPnSourceAddress to honour immediate
  flips (feature 021). 2.5: HdrFP16 with HdrARGB10 is fatal; HwQueueSupported
  contexts allowed. 2.6: nothing for this driver. 2.7: ControlInterrupt3
  replaces ControlInterrupt2 when registered; black-screen
  `CollectDiagnosticInfo`.
- Q (agent, RE): Which of the twelve slots? -> A: None implemented; each NULL is
  safe. `UpdateHwContextState` (129) is never copied.
  `Create/DestroyProtectedSession` (130/131): NULL answers applications
  NOT_SUPPORTED, which is true - no protected content is enforced.
  `SignalMonitoredFence` (144, no NULL check) and `PresentToHwQueue` (145) are
  reached only on hardware-scheduled nodes; 144 arrives with enablement, once its
  DMA buffer contract is traced. `ValidateSubmitCommand` (146) is called only for
  contexts or queues with private data, which this driver reports none of.
  `SetTargetAdjustedColorimetry2` (147) falls back to 123;
  `SetTrackedWorkloadPowerLevel` (148) needs `SupportTrackedWorkload`;
  `Save/RestoreMemoryForHotUpdate` (149/150) need the GPU partition interface;
  `CollectDiagnosticInfo` (151) is NULL-checked and may be called with no
  adapter after a failed AddDevice; `ControlInterrupt3` (153) only replaces
  ControlInterrupt2 from 2.7.
- Q (agent): WDDMVersion? -> A: 2.3.

## User Scenarios & Testing *(mandatory)*

Users: dxgkrnl's adapter start and its display state synchronization.

### User Story 1 - Starting a WDDM 2.3 adapter (Priority: P1)

1. **Given** start, **Then** DRIVERCAPS reports 2.3, every capability check
   passes, and the twelve slots are NULL.

## Requirements *(mandatory)*

- **FR-001**: The only capability change is WDDMVersion 2.3.

## Success Criteria *(mandatory)*

- **SC-001**: Through the table: start at 2.3 with the twelve slots NULL, a mode
  set, a primary shown and hidden at vsync, no violation.
- **SC-002**: 100% branch coverage.

## Out of Scope

- Protected sessions, tracked workloads, hot driver update, diagnostics
  collection, ControlInterrupt3, and hardware-scheduled fences and presents.
