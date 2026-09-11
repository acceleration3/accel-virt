# Feature Specification: WDDM 1.0: capability reporting

**Feature Branch**: `007-wddm10-capability-reporting`

**Created**: 2026-09-10

**Status**: Implemented

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 007 · **Status**: `implement` · **Depends on**: 006
· **WDDM level**: WDDM 1.0 (Windows Vista)

DxgkDdiQueryAdapterInfo, the 48-way switch that decides what kind of driver
this is. Wrong answers here fail silently much later, so this feature owns the
rule that a capability is never reported before it is implemented.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

### DDI slots in scope (3)

| Slot | Field | Introduced |
|--:|---|---|
| 17 | `DxgkDdiQueryAdapterInfo` | BASE |
| 34 | `DxgkDdiEscape` | BASE |
| 35 | `DxgkDdiCollectDbgInfo` | BASE |

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
| `AdapterInfoQuery` | object | platform, one `DXGKARG_QUERYADAPTERINFO` | Bounds one QueryAdapterInfo request's buffers |
| `DriverCapabilities` | object | platform, WDDM version, highest acceptable address | Decides every capability the adapter reports |
| `DebugReport` | object | platform, started, powered, negotiated features | Serializes one adapter state snapshot for a TDR report |
| `DxgkargQueryAdapterInfoRecord`, `DxgkDriverCapsRecord`, `DxgkWddmDeviceCapsRecord`, `DxgkargEscapeRecord`, `DxgkargCollectDbgInfoRecord`, `DxgkargCollectDbgInfoExtRecord` | DDI record | the fields of one WDK structure | Overlays memory dxgkrnl owns, layout-proven in `src/km/ddi_record_layout.cpp` |
| `FakeDxgadapter` (harness) | object | platform, table, adapter context, last `DRIVERCAPS`, failed rule | Validates capabilities the way `DXGADAPTER::Initialize` does |

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

- [`docs/kb/02-kmd-contract.md`](../../docs/kb/02-kmd-contract.md)
- [`docs/kb/04-memory.md`](../../docs/kb/04-memory.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Evidence

Derived from `dxgkrnl.sys` 10.0.26100.9444 and `basicrender.sys` in
`re/ghidra-proj`, and from `d3dkmddi.h` (10.0.26100):

- `DXGADAPTER::DdiQueryAdapterInfo @ 1402f4e90` forwards every `Type`
  unfiltered and logs "Driver returned an invalid NTSTATUS code" for any result
  other than `STATUS_SUCCESS`, `STATUS_NOT_IMPLEMENTED` (0xC0000002),
  `STATUS_INVALID_PARAMETER` (0xC000000D) or `STATUS_NO_MEMORY` (0xC0000017).
  `docs/testing.md` and `docs/ddi-sequences.md` §5, which name
  `STATUS_NOT_SUPPORTED`, are wrong and are corrected by this feature.
- `DXGADAPTER::Initialize @ 1401a2818` asks for `DXGKQAITYPE_DRIVERCAPS` first,
  with an output size of `GetDriverCapsSizeFromDdiVersion(0x11007)` = 0x250
  bytes; failure fails adapter start. It then requires `WDDMVersion` to be one
  of the `DXGK_WDDMVERSION` values; asks `PHYSICALADAPTERCAPS` only when
  `WDDMVersion` >= 2.0 and then requires `SchedulingCaps.MultiEngineAware`;
  rejects a pre-1.2 version that sets any 1.2 capability, a pre-1.3 version
  that sets any 1.3 capability, `MiscCaps.Reserved` bits, and
  `IndependentVidPnVSyncControl` / `SupportPerEngineTDR` /
  `SupportKernelModeCommandBuffer` / `SupportRuntimePowerManagement` /
  `SupportMultiPlaneOverlay` without the DDIs each needs.
- `WarpKMQueryAdapterInfo` answers an output smaller than 0x250 for
  `DRIVERCAPS`, and any type it does not know, with `STATUS_INVALID_PARAMETER`.
  `WarpKMEscape` answers an escape code it does not know with
  `STATUS_NOT_SUPPORTED`.
- `DXGK_WDDMDEVICECAPS` (`DXGKQAITYPE_WDDMDEVICECAPS`) is "queried by the OS
  during device initialization after DxgkDdiAddDevice returns and before
  DxgkDdiStartDevice is called", and carries the same `WDDMVersion` as
  `DRIVERCAPS`.

## User Scenarios & Testing *(mandatory)*

Users: dxgkrnl deciding what kind of adapter this is; every later feature,
which turns capabilities on as it implements them; a reviewer asking "does the
driver claim anything it does not do".

### User Story 1 - dxgkrnl learns a truthful capability set (Priority: P1)

**Independent Test**: the fake dxgkrnl runs the recorded start sequence up to
and including the `DRIVERCAPS` query and applies `DXGADAPTER::Initialize`'s
validation to the answer; start proceeds, and every capability field is zero
except those this feature documents.

**Acceptance Scenarios**:

1. **Given** a `DRIVERCAPS` query with an output of at least 0x250 bytes,
   **When** the adapter answers, **Then** it succeeds, writes exactly 0x250
   bytes, reports `WDDMVersion` = `DXGKDDI_WDDMv1` (0x1000),
   `HighestAcceptableAddress` = all ones, and zero in every other field.
2. **Given** the answer, **When** the fake dxgkrnl validates it, **Then** no
   `DXGADAPTER::Initialize` rule rejects it.
3. **Given** a `DRIVERCAPS` output smaller than 0x250 bytes, or a null output,
   **Then** `STATUS_INVALID_PARAMETER` and nothing is written.
4. **Given** `DRIVERCAPS` before StartDevice, after StopDevice, or at D3,
   **Then** the same answer: capabilities describe the driver, not the
   device's power state.
5. **Given** a `WDDMDEVICECAPS` query with a 4-byte input and at least a
   4-byte output, **Then** success and the same `WDDMVersion` as `DRIVERCAPS`;
   a short input or output is `STATUS_INVALID_PARAMETER`.

### User Story 2 - Every other query is declined with a status dxgkrnl accepts (Priority: P1)

**Independent Test**: every `DXGKQAITYPE` value 0-48, plus values outside the
enumeration, is sent through the registered table with garbage buffers.

**Acceptance Scenarios**:

1. **Given** any type this feature does not answer, **Then**
   `STATUS_NOT_IMPLEMENTED` and neither buffer is read or written.
2. **Given** every answer to every type, **Then** its status is one of the four
   dxgkrnl accepts.
3. **Given** a null argument block, **Then** `STATUS_INVALID_PARAMETER`.

### User Story 3 - A capability cannot be reported before it is implemented (Priority: P1)

**Independent Test**: the rule is a test that fails when a capability is
turned on without the DDI slots it depends on being registered.

**Acceptance Scenarios**:

1. **Given** the registered DDI table, **When** the capability answer is
   compared to it, **Then** each capability the fake dxgkrnl ties to DDIs is
   set only if those slots are non-NULL, and `WDDMVersion` claims no level
   whose mandatory capabilities are absent.
2. **Given** a later feature sets a capability, **When** its DDIs are missing,
   **Then** the scenario test fails naming the capability.

### User Story 4 - Escape and debug collection are safe on hostile input (Priority: P2)

**Acceptance Scenarios**:

1. **Given** `DxgkDdiEscape` with null private data, zero size, or any size,
   **Then** `STATUS_INVALID_PARAMETER` for null/zero and
   `STATUS_NOT_SUPPORTED` otherwise, reading nothing past the declared size;
   no escape code is defined yet.
2. **Given** `DxgkDdiCollectDbgInfo` with a buffer of any size including zero,
   **Then** it writes a bounded snapshot of the adapter's state (a
   recognizable header, whether the adapter is started, the device status)
   truncated to the buffer, never past it, and succeeds; a null buffer with a
   non-zero size is `STATUS_INVALID_PARAMETER`; a non-null extension has its
   output fields zeroed.

### Edge Cases

- `OutputDataSize` enormous with a small real buffer: only the documented
  bytes are written.
- `Type` negative or above 48: declined like any unanswered type.
- QueryAdapterInfo on an adapter whose start failed: still answers.
- `CollectDbgInfo` during a stop: snapshot reflects stopped.

## Requirements *(mandatory)*

- **FR-001**: QueryAdapterInfo MUST answer only with `STATUS_SUCCESS`,
  `STATUS_NOT_IMPLEMENTED`, `STATUS_INVALID_PARAMETER` or `STATUS_NO_MEMORY`.
- **FR-002**: `DRIVERCAPS` MUST be answered at any adapter state, into exactly
  0x250 bytes, with `WDDMVersion` 0x1000 and no capability bit set.
- **FR-003**: Every capability MUST have one owner: a single place that
  decides its value, which later features change as they implement it.
- **FR-004**: The fake dxgkrnl MUST apply `DXGADAPTER::Initialize`'s
  capability validation, so a later feature claiming a capability without its
  DDIs fails a test.
- **FR-005**: Every DDI argument block read by these three slots MUST be
  treated as hostile: sizes checked before any read or write, fuzzed.
- **FR-006**: `CollectDbgInfo` MUST NOT allocate, block or take a lock, since
  it runs while the adapter is being recovered.
- **FR-007**: The three slots MUST be registered and driven through the table
  by the harness.
- **FR-008**: Docs that name `STATUS_NOT_SUPPORTED` for unanswered query
  types MUST be corrected.

## Success Criteria *(mandatory)*

- **SC-001**: The fake dxgkrnl start scenario passes the `DRIVERCAPS`
  validation with no leak or violation.
- **SC-002**: All 49 query types and out-of-range types return an accepted
  status; zero writes outside declared buffers under the hostile sweep.
- **SC-003**: 100% branch coverage of added core code; the argument-block
  fuzz target runs clean for the 60-second smoke.

## Assumptions

- `HighestAcceptableAddress` all ones is truthful: the virtio device
  addresses all guest physical memory (no IOMMU restriction), as
  `basicrender.sys` reports.
- `STATUS_NOT_IMPLEMENTED` is the decline: it is in dxgkrnl's accepted set and
  says what is true. `basicrender.sys`'s `STATUS_INVALID_PARAMETER` is
  reserved here for malformed arguments.
- No escape protocol exists until a user-mode component needs one.

## Out of Scope

- Segments (`QUERYSEGMENT*`, 008), node count and scheduling caps (009),
  pointer caps (013), GPU MMU caps (017), native fence and user-mode
  submission caps (024), and every other capability: each arrives with the
  feature that implements it.
- `UMDRIVERPRIVATE`: no user-mode driver yet.

