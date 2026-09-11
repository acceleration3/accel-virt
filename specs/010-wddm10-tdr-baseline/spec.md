# Feature Specification: WDDM 1.0: timeout detection and recovery

**Feature Branch**: `010-wddm10-tdr-baseline`

**Created**: 2026-09-10

**Status**: Implemented

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 010 · **Status**: `implement` · **Depends on**: 009
· **WDDM level**: WDDM 1.0 (Windows Vista)

The watchdog does not know there is no hardware. Progress tracking, reset and
restart must be real from the first submission, not added later.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

### DDI slots in scope (2)

| Slot | Field | Introduced |
|--:|---|---|
| 32 | `DxgkDdiResetFromTimeout` | BASE |
| 33 | `DxgkDdiRestartFromTimeout` | BASE |

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
| `ExecutionNode` (extended) | object | + stopped, time of last progress | Tracks one node's packets from submission to completion, across a reset |
| `DebugReport` (extended) | object | + last submitted and completed fence, pending count, time of last progress | Serializes one adapter state snapshot for a TDR report |

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

- Q (agent, RE): What does dxgkrnl accept from the two DDIs? ->
  A: `ADAPTER_RENDER::DdiResetFromTimeout @ 1401beff8` and
  `DdiRestartFromTimeout @ 140425574` call slots 32 and 33 under the adapter's
  DDI sync lock and log any status other than `STATUS_SUCCESS` as "Driver
  returned an invalid NTSTATUS code"; `VidSchiRecoverFromTDR @ 140093834`
  (dxgmms2) reaches them through `TdrResetFromTimeoutAsync`. Both are PASSIVE.
  `basicrender.sys` resets by flagging its worker and waiting for it to stop.
- Q (agent): What does a reset do to packets and the device? -> A: VidSch
  aborts every packet itself, so the driver drops its queue without reporting
  anything and refuses submissions until restart. The virtio device is reset
  (the D3 path) and brought back on restart (the D0 path), because after a
  timeout nothing the device holds can be trusted; host state that must be
  re-created arrives with the features that create it.
- Q (agent): What is "progress tracking" at this level? -> A: The node records
  when it last made progress (a submission to an idle queue, or a completion),
  and CollectDbgInfo's report carries it with the last submitted and completed
  fences, so a TDR dump says what the engine was doing. The engine-status query
  that consumes it is WDDM 1.2 (feature 015).

## User Scenarios & Testing *(mandatory)*

Users: dxgkrnl's TDR recovery; whoever reads the TDR dump.

### User Story 1 - Reset and restart (Priority: P1)

1. **Given** packets submitted and no DPC yet, **When** ResetFromTimeout runs,
   **Then** it succeeds, the queue is empty, a DPC reports nothing, the virtio
   device is reset, and SubmitCommand is refused until restart.
2. **When** RestartFromTimeout runs, **Then** the device is back at DRIVER_OK,
   a submission with a fence above the last one submitted is accepted and
   completes, and the current fence moves on from there.
3. **Given** reset then restart twice, **Then** the same holds each time with
   no leak.
4. **Given** reset or restart on an adapter that never started, **Then**
   success and no device access.
5. **Given** a device that will not come back on restart, **Then** the failure
   status is returned (dxgkrnl will escalate).

### User Story 2 - The TDR report says where the engine was (Priority: P2)

1. **Given** CollectDbgInfo, **Then** the report adds the last submitted fence,
   the last completed fence, the pending count and the time of last progress,
   in the documented layout.

## Requirements *(mandatory)*

- **FR-001**: Reset MUST drop pending packets without notifying dxgkrnl.
- **FR-002**: Between reset and restart, SubmitCommand MUST be refused and the
  DPC MUST report nothing.
- **FR-003**: Progress time MUST come from the platform's interrupt time, so a
  test controls it.
- **FR-004**: The fake dxgkrnl scenario MUST run reset/restart twice (docs/testing.md).

## Success Criteria *(mandatory)*

- **SC-001**: Scenario: submit, time out, reset, restart, submit and complete,
  twice, with no violation or leak.
- **SC-002**: 100% branch coverage; the render core rule's first missing slot
  becomes ControlInterrupt.

## Out of Scope

- Engine status, per-engine reset and dependent engine groups (015).
- Re-creating host state after reset (the device features, 029-033).
