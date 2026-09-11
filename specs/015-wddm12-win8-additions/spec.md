# Feature Specification: WDDM 1.2 (Windows 8) additions

**Feature Branch**: `015-wddm12-win8-additions`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 015 · **Status**: `implement` · **Depends on**: 014
· **WDDM level**: WDDM 1.2 (Windows 8)

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

### DDI slots in scope (12)

| Slot | Field | Introduced |
|--:|---|---|
| 71 | `DxgkDdiSetPowerComponentFState` | WIN8 |
| 72 | `DxgkDdiQueryDependentEngineGroup` | WIN8 |
| 73 | `DxgkDdiQueryEngineStatus` | WIN8 |
| 74 | `DxgkDdiResetEngine` | WIN8 |
| 75 | `DxgkDdiStopDeviceAndReleasePostDisplayOwnership` | WIN8 |
| 76 | `DxgkDdiSystemDisplayEnable` | WIN8 |
| 77 | `DxgkDdiSystemDisplayWrite` | WIN8 |
| 78 | `DxgkDdiCancelCommand` | WIN8 |
| 79 | `DxgkDdiGetChildContainerId` | WIN8 |
| 80 | `DxgkDdiPowerRuntimeControlRequest` | WIN8 |
| 81 | `DxgkDdiSetVidPnSourceAddressWithMultiPlaneOverlay` | WIN8 |
| 82 | `DxgkDdiNotifySurpriseRemoval` | WIN8 |

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
| `ExecutionNode` (extended) | object | + abort of what is pending, keeping the node running | The one node's queue of submitted fences |
| `Edid` (extended) | object | + the ELD fields of the synthetic monitor | Answers descriptor queries for the synthetic monitor |
| `Adapter` (extended) | object | + whether the device is still present | One PCI display device |
| DDI records | DDI record | `DXGKARG_QUERYDEPENDENTENGINEGROUP`, `DXGKARG_QUERYENGINESTATUS`, `DXGKARG_RESETENGINE`, `DXGKARG_CANCELCOMMAND`, `DXGK_CHILD_CONTAINER_ID`, `DXGK_DISPLAY_INFORMATION` | Overlays memory dxgkrnl owns, layout-proven |

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

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11

- Q (agent, RE): Do these DDIs change what the adapter claims? -> A: No. Each
  capability that makes dxgkrnl use them - `SupportPerEngineTDR`,
  `CancelCommandAware`, `SupportRuntimePowerManagement`,
  `SupportMultiPlaneOverlay`, `SupportSurpriseRemovalInHibernation` - is a WDDM
  1.2+ claim, and `DXGADAPTER::Initialize` refuses a 1.2 claim below
  `DXGKDDI_WDDMv1_2` while a full 1.2 adapter must also set
  `SupportDirectFlip`. The WDDM version is raised with GPU virtual addressing
  (feature 017); until then the DDIs are implemented, tested and registered but
  unclaimed. `DXGK_DRIVERCAPS` is unchanged.
- Q (agent, RE): Per-engine TDR? -> A: As `basicrender.sys`:
  `QueryDependentEngineGroup` answers `1 << NodeOrdinal`; `QueryEngineStatus`
  sets `Responsive`; `ResetEngine` abandons what the node was executing and
  answers its fence in `LastAbortedFenceId` (the last completed fence when
  nothing was pending), leaving the node running; `CancelCommand` has nothing to
  free for a buffer that was never submitted. Only node 0, engine 0 exists.
- Q (agent, RE): StopDeviceAndReleasePostDisplayOwnership? -> A:
  `DpiFdoStopDeviceAndReleasePostDisplayOwnership` calls it instead of StopDevice
  for a POST display owner. A `DXGK_DISPLAY_INFORMATION` of width 0 is accepted
  and means no frame buffer is handed to the basic display driver: this adapter's
  scan-out is guest pages behind a GPU address, not a physical frame buffer, so
  it stops the device and answers that.
- Q (agent, RE): Surprise removal? -> A: `DpiFdoHandleSurpriseRemoval` calls
  `NotifySurpriseRemoval` (`DxgkRemovalPnPNotify`) when the device is gone, then
  StopDevice. The adapter stops touching the device: the ISR claims nothing, the
  DPC harvests nothing, and stopping skips the transport reset that would wait
  on a device that cannot answer.
- Q (agent, RE): System display (bugcheck screen)? -> A: `DpiEnterSystemDisplay`
  tries each WIN8 miniport's `SystemDisplayEnable` and falls back to the POST
  adapter on failure. Writing at HIGH_LEVEL would need a frame buffer resource
  and lock-free virtqueue polling this driver does not have; SystemDisplayEnable
  answers `STATUS_NOT_SUPPORTED` and SystemDisplayWrite, never reached after that,
  does nothing.
- Q (agent): Power components? -> A: `DXGKQAITYPE_POWERCOMPONENTINFO` is not
  answered, so no component exists: `SetPowerComponentFState` is
  `STATUS_INVALID_PARAMETER` for any index; `PowerRuntimeControlRequest` is
  `STATUS_NOT_SUPPORTED`.
- Q (agent): Container id? -> A: For child 0, the ELD manufacturer and product of
  the synthetic EDID ("RVK", product 1) and port 0; the container GUID dxgkrnl
  passed in is left as it is.
- Q (agent): Multi-plane overlay? -> A: `STATUS_NOT_SUPPORTED`: no overlay planes.

## User Scenarios & Testing *(mandatory)*

Users: dxgkrnl's TDR engine, PnP surprise removal, the POST display hand-off,
bugcheck display, runtime power and the monitor stack's container ids.

### User Story 1 - Engine recovery (Priority: P1)

1. **Given** two submitted fences, **When** ResetEngine for node 0, **Then**
   `LastAbortedFenceId` is the first, the queue is empty, and a new submission is
   accepted; with nothing pending it is the last completed fence.
2. QueryEngineStatus reports responsive; QueryDependentEngineGroup reports mask
   1; another node or engine is `STATUS_INVALID_PARAMETER`.
3. CancelCommand on a live context succeeds; on an unknown context it is
   `STATUS_INVALID_PARAMETER`.

### User Story 2 - Removal and hand-off (Priority: P1)

1. **Given** a started adapter, **When** NotifySurpriseRemoval then StopDevice,
   **Then** the ISR claims nothing, the DPC harvests nothing, StopDevice does not
   wait on the device, and no resource leaks.
2. StopDeviceAndReleasePostDisplayOwnership stops the device and answers a zero
   display information.

### User Story 3 - The rest (Priority: P2)

1. GetChildContainerId fills the ELD for child 0 only.
2. SystemDisplayEnable, PowerRuntimeControlRequest and
   SetVidPnSourceAddressWithMultiPlaneOverlay decline; SetPowerComponentFState
   refuses every index.

## Requirements *(mandatory)*

- **FR-001**: No capability changes.
- **FR-002**: After surprise removal nothing reads or writes device registers
  except the final abandon on destruction.

## Success Criteria *(mandatory)*

- **SC-001**: Through the table: submissions, engine reset, removal and stop -
  no leak, no violation, the render core still created.
- **SC-002**: 100% branch coverage.

## Out of Scope

- Claiming WDDM 1.2 (with GPU VA, feature 017); system display; runtime power
  components; overlays.
