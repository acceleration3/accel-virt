# Feature Specification: WDDM 2.1 additions

**Feature Branch**: `018-wddm21-additions`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 018 · **Status**: `partial` · **Depends on**: 017
· **WDDM level**: WDDM 2.1 (1607)

### Status meaning

Implement only the subset listed below. Everything else in this level is stubbed or skipped, and the stub is tested.

### DDI slots in scope (6)

| Slot | Field | Introduced |
|--:|---|---|
| 104 | `DxgkDdiCheckMultiPlaneOverlaySupport3` | WDDM2_1 |
| 105 | `DxgkDdiSetVidPnSourceAddressWithMultiPlaneOverlay3` | WDDM2_1 |
| 106 | `DxgkDdiPostMultiPlaneOverlayPresent` | WDDM2_1 |
| 107 | `DxgkDdiValidateUpdateAllocationProperty` | WDDM2_1 |
| 108 | `DxgkDdiControlModeBehavior` | WDDM2_1 |
| 109 | `DxgkDdiUpdateMonitorLinkInfo` | WDDM2_1 |

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
| `DriverCapabilities` (extended) | object | + WDDM 2.1, the mode behaviors this driver satisfies (none) | Reports what the driver implements |
| `Edid` (extended) | object | the EDID block, now also read for the link capabilities | Describes the one monitor |
| DDI records | DDI record | `DXGKARG_CONTROLMODEBEHAVIOR`, `DXGKARG_UPDATEMONITORLINKINFO` | Overlays memory dxgkrnl owns, layout-proven |
| `FakeDxgmonitor` (harness) | object | table, adapter, driver version, the link info dxgkrnl kept | `DXGMONITOR::_UpdateLinkInfo`'s call and its handling of the answer |

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

- Q (agent, RE): Which of the six WDDM 2.1 slots? -> A: Implemented:
  `ControlModeBehavior` (108), because `DXGADAPTER::Initialize @ 1401a2e21`
  calls it on every start of an adapter with a source and a child, before
  DRIVERCAPS (`SetModeBehavior @ 1401a8d0c`, Request PrioritizeHDR for DDI
  interface >= 0x7000; the answer is only traced, a status other than
  NOT_SUPPORTED only logged, and the wrapper asserts Satisfied and NotSatisfied
  share no bit); and `UpdateMonitorLinkInfo` (109), called by
  `DXGMONITOR::_UpdateLinkInfo @ 14028e4a0` on monitor arrival and usage change.
  NULL is replaced by a default answering STATUS_NOT_SUPPORTED for both, which
  is swallowed below driver version 2400 but not from 2.4. Skipped, NULL:
  `CheckMultiPlaneOverlaySupport3`, `SetVidPnSourceAddressWithMultiPlaneOverlay3`
  (only with `SupportMultiPlaneOverlay`, which is not reported, and the MPO3 flip
  flag needs 105 non-NULL), `PostMultiPlaneOverlayPresent` (no caller in this
  build) and `ValidateUpdateAllocationProperty` (NULL-checked:
  `NtGdiDdDDIUpdateAllocationProperty` answers STATUS_NOT_IMPLEMENTED; no user
  mode driver calls it yet).
- Q (agent): ControlModeBehavior's answer? -> A: No behavior is satisfied:
  Satisfied 0, NotSatisfied the whole request, STATUS_SUCCESS, at any adapter
  state (it arrives after StartDevice, before the capability queries).
- Q (agent): UpdateMonitorLinkInfo's answer? -> A: For target 0: the usage
  hints left as the OS gave them, capabilities as the EDID describes the monitor
  (no stereo in its detailed timing, no colour extension block: 0), dithering
  support 0. Another target is STATUS_INVALID_PARAMETER.
- Q (agent, RE): WDDMVersion 2.1? -> A: Reported. `Initialize` accepts 0x2100;
  no start query, capability or DDI becomes mandatory; user mode sees driver
  version 2100. The only behavioural change (`ReadConfig @ 1401a84bd`) is the
  secondary independent flip, which needs `FlipIndependent`, not reported.

## User Scenarios & Testing *(mandatory)*

Users: dxgkrnl's adapter start (mode behavior), its monitor objects (link
information for HDR and wide colour decisions).

### User Story 1 - Starting a WDDM 2.1 adapter (Priority: P1)

1. **Given** a started adapter, **When** dxgkrnl's Initialize runs, **Then**
   ControlModeBehavior answers PrioritizeHDR not satisfied, DRIVERCAPS reports
   2.1 and every capability check passes.

### User Story 2 - Monitor link information (Priority: P1)

1. **Given** the monitor on target 0, **When** dxgkrnl updates its link info with
   usage hints Hidden, **Then** the capabilities and dithering support are 0 and
   the hints come back unchanged; target 1 is refused.

## Requirements *(mandatory)*

- **FR-001**: The four skipped slots are NULL and registration succeeds.
- **FR-002**: The only capability change is WDDMVersion 2.1.

## Success Criteria *(mandatory)*

- **SC-001**: Through the table: start with mode behavior, a monitor link update
  accepted by the fake DXGMONITOR, no violation.
- **SC-002**: 100% branch coverage.

## Out of Scope

- Multi-plane overlay 3, allocation property updates, HDR and wide colour.
