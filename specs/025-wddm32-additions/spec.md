# Feature Specification: WDDM 3.2: native fence sharing, and declining GPU-P and live migration

**Feature Branch**: `025-wddm32-additions`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 025 · **Status**: `partial` · **Depends on**: 024
· **WDDM level**: WDDM 3.2 (24H2)

Implement native fence sharing, ResetDisplayEngine and CollectDbgInfo2.
Decline the SR-IOV/partitioning family outright — it describes mediated
hardware passthrough, which this design is not. Live migration and dirty
tracking are deferred, not declined.

### Status meaning

Implement only the subset listed below. Everything else in this level is stubbed or skipped, and the stub is tested.

### DDI slots in scope (21)

| Slot | Field | Introduced |
|--:|---|---|
| 172 | `DxgkDdiCreateMemoryBasis` | WDDM3_2 |
| 173 | `DxgkDdiDestroyMemoryBasis` | WDDM3_2 |
| 174 | `DxgkDdiStartDirtyTracking` | WDDM3_2 |
| 175 | `DxgkDdiStopDirtyTracking` | WDDM3_2 |
| 176 | `DxgkDdiQueryDirtyBitData` | WDDM3_2 |
| 177 | `DxgkDdiPrepareLiveMigration` | WDDM3_2 |
| 178 | `DxgkDdiSaveImmutableMigrationData` | WDDM3_2 |
| 179 | `DxgkDdiSaveMutableMigrationData` | WDDM3_2 |
| 180 | `DxgkDdiEndLiveMigration` | WDDM3_2 |
| 181 | `DxgkDdiRestoreImmutableMigrationData` | WDDM3_2 |
| 182 | `DxgkDdiRestoreMutableMigrationData` | WDDM3_2 |
| 183 | `DxgkDdiWriteVirtualizedInterrupt` | WDDM3_2 |
| 184 | `DxgkDdiSetVirtualGpuResources2` | WDDM3_2 |
| 185 | `DxgkDdiSetVirtualFunctionPauseState` | WDDM3_2 |
| 186 | `DxgkDdiOpenNativeFence` | WDDM3_2 |
| 187 | `DxgkDdiCloseNativeFence` | WDDM3_2 |
| 188 | `DxgkDdiSetNativeFenceLogBuffer` | WDDM3_2 |
| 189 | `DxgkDdiUpdateNativeFenceLogs` | WDDM3_2 |
| 190 | `DxgkDdiCollectDbgInfo2` | WDDM3_2 |
| 191 | `DxgkDdiNotifyContextPriorityChange` | WDDM3_2 |
| 192 | `DxgkDdiResetDisplayEngine` | WDDM3_2 |

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
| `DriverCapabilities` (extended) | object | WDDM 3.2 | Reports what the driver implements |
| `FakeDxgadapter` (harness, extended) | object | + the 3.2 display rule | DXGADAPTER::Initialize and DpiFdoStartAdapter's capability checks |

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
- [`docs/kb/09-gpupv.md`](../../docs/kb/09-gpupv.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11

- Q (agent, RE): What does WDDM 3.2 require? -> A: For a display adapter,
  `UpdateMonitorLinkInfo` (`DpiFdoStartAdapter @ 14025b7b4`, else
  STATUS_REVISION_MISMATCH) - implemented in feature 018. The dirty bit tracking
  query (39) is asked and a failure only logged; its DirtyBitTracking answer would
  make slots 172-176 mandatory (`CreateRenderCore @ 1401b1a74`). The render core
  takes CollectDbgInfo or CollectDbgInfo2. Nothing else.
- Q (agent, RE): Which of the twenty-one slots? -> A: None; all NULL. Memory basis
  and dirty tracking (172-176) are reached only from GPU partitioning; live
  migration (177-183) and the virtual function pair (184-185) only with the GPU-P
  interface, which needs all of 177-185; `Open/CloseNativeFence` and the native
  fence logs (186-189) only with native fences (feature 024);
  `CollectDbgInfo2` (190) would replace the implemented CollectDbgInfo;
  `NotifyContextPriorityChange` (191) is NULL-checked and answers success;
  `ResetDisplayEngine` (192) has no call site in this build. `basicrender.sys`
  reports 3.2 with every slot above 146 NULL.
- Q (agent): Query 39? -> A: Not answered (STATUS_NOT_IMPLEMENTED), as
  `basicrender.sys` declines it: no dirty bit tracking.
- Q (agent): WDDMVersion? -> A: 3.2, the level `rvkgpu.sys` has declared as its
  DDI interface since feature 006.

## User Scenarios & Testing *(mandatory)*

Users: dxgkrnl's adapter start.

### User Story 1 - Starting a WDDM 3.2 adapter (Priority: P1)

1. **Given** start, **Then** DRIVERCAPS reports 3.2, the dirty bit tracking query
   is declined, the display adapter's UpdateMonitorLinkInfo satisfies the 3.2
   rule, and the twenty-one slots are NULL.

## Requirements *(mandatory)*

- **FR-001**: The only capability change is WDDMVersion 3.2.

## Success Criteria *(mandatory)*

- **SC-001**: Through the table: start at 3.2 with the twenty-one slots NULL, a
  mode set and a monitor link update, no violation.
- **SC-002**: 100% branch coverage.

## Out of Scope

- GPU partitioning, live migration, dirty bit tracking, native fence sharing and
  logs, CollectDbgInfo2, display engine reset.
