# Feature Specification: Real-guest power, restart and recovery

**Feature Branch**: `040-guest-power-and-recovery`

**Created**: 2026-09-11

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 040 · **Status**: `implement` · **Depends on**: 038, 033

The adapter survives what Windows does to a GPU: sleep and hibernate with
resume, device disable and enable, a driver upgrade without reboot, TDR on a
stalled host context that recovers only that device, and a host executor that
faults - each exercised by the in-guest runner, not only the harness.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

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
| `EngineProgress` | object | the interrupt time and node progress of its last answer, and whether it has answered | Say whether the engine is making progress |
| `PowerTransition` | object | whether the device was started when power went away, and which sources were scanning out | Put the adapter back the way power found it |
| `ContextLoss` | system | the contexts nobody can finish, and how many of them their owners have been told about | Report a context nobody can finish |

### Test requirements

Constitution Principle III applies in full. Concretely, this feature is not done until:

- every branch of every function it adds to `src/core`, `src/device` or `src/proto` is covered, or marked `AV_UNTESTABLE` with a justification;
- each stateful module it adds exports an invariant function that tests call after **every** mutation;
- every function it adds that reads bytes from outside its own address space has a hostile-input table test and a libFuzzer target with a committed corpus;
- the allocation-failure sweep passes: with each allocation site failed in turn, no crash and no leak;
- IRQL annotations are present and the mock's IRQL checker is satisfied;
- `tools/check_symbol_tests.py` reports no untested exported symbol.

### Reference material

Knowledge base:

- [`docs/kb/05-scheduling.md`](../../docs/kb/05-scheduling.md)
- [`docs/kb/02-kmd-contract.md`](../../docs/kb/02-kmd-contract.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## User Scenarios & Testing *(mandatory)*

Users: anyone whose guest sleeps, hibernates, updates its driver or hits a host
stall. Windows does these things to every GPU; a driver that survives only a
clean boot is not a driver. kb 05: a remoting driver trips the 2-second TDR the
first time the host stalls, so `QueryEngineStatus` must reflect host progress
and reset must resynchronize the guest's object graph with the host.

### User Story 1 - Sleep and hibernate (Priority: P1)

**Independent Test**: L5 runner scenarios suspend through the guest agent
(`guest-suspend-ram`, `guest-suspend-disk`), wake with QMP `system_wakeup` or
by booting the same overlay again, then run the check stage and `rvktest.exe`.

**Acceptance Scenarios**:

1. **Given** the running adapter, **When** the guest enters S3 and wakes,
   **Then** the adapter returns to D0, the desktop is restored, and
   `rvktest.exe` passes.
2. **Given** the running adapter, **When** the guest hibernates and the overlay
   boots again on the same QEMU configuration, **Then** it resumes with the
   adapter started and `rvktest.exe` passes.
3. **Given** contexts that existed before the transition, **Then** they are
   either restored or reported lost to their owners; no stale ring or fence
   address is used.

### User Story 2 - Disable, enable and upgrade without reboot (Priority: P1)

**Independent Test**: L5 scenarios using `pnputil /disable-device`,
`/enable-device` and `/add-driver /install` with a newer package version.

**Acceptance Scenarios**:

1. **Given** the running adapter, **When** it is disabled and enabled 50 times,
   **Then** each enable starts it, and after the last disable Driver Verifier's
   pool tracking shows no outstanding `rvkg` allocation.
2. **Given** a newer package, **When** it is installed, **Then** the driver is
   replaced without a reboot and the adapter starts on the new version.

### User Story 3 - A stalled host context is recovered alone (Priority: P1)

**Independent Test**: L2 scenarios of the TDR sequence, and an L5 scenario in
which the executor is made to stall one context longer than the TDR timeout.

**Acceptance Scenarios**:

1. **Given** two processes with contexts, **When** one context's host work
   stalls for 5 s, **Then** `dxgkrnl` detects the timeout, only that context's
   engine is reset, the other process keeps completing work, the System log
   holds event 4101, and a live kernel report reaches `diag/dumps/`.
2. **Given** a host that is slow but progressing, **Then** no TDR occurs:
   engine status reflects host progress, not ring occupancy.

### User Story 4 - A faulting executor (Priority: P2)

**Independent Test**: L2 and L4 scenarios with a faulting mock executor; an L5
scenario with the executor made to fault one context.

**Acceptance Scenarios**:

1. **Given** the host reports a fault for one context, **Then** the driver
   reports the DMA as faulted, only that device is reset, and the adapter stays
   up.

> **Superseded (044):** the driver never reports `DMA_FAULTED` - dxgmms2.sys `VidSchDdiNotifyInterruptWorker` @140031b80 bugchecks 0x119/0xD on `DXGK_INTERRUPT_DMA_FAULTED` from a GPU MMU adapter; a faulted context's queued work is completed, its later submissions are refused, ResetEngine reports nothing more, and `ContextLoss` is deleted (`docs/kb/05-scheduling.md`).
2. **Given** a device realized without an executor, **Then** the adapter starts
   with the 2D path only and refuses rvk contexts with a reported status.

### Edge Cases

- Hibernate, then boot on a QEMU whose device changed: device removed (rescue
  semantics), a different host GPU UUID (contexts refused, desktop works), or
  an older protocol version.
- Suspend while a submission is in flight, or with doorbells connected
  (`DisconnectDoorbell` honoured).
- Disable while an application holds allocations and contexts.
- Repeated TDRs within Windows' window: escalation happens only if the reset
  truly fails.
- A host GPU reset while the guest is suspended.
- A guest that never wakes: the runner's timeout names the step.
- S3 not offered by the QEMU configuration: the scenario reports it skipped.

## Requirements *(mandatory)*

- **FR-001**: Every power and reset DDI is driven at L2 through the abusive but
  legal sequences `dxgkrnl` produces, citing the `dxgkrnl` functions read, and
  at L5 through S3, S4, disable and enable, and upgrade.
- **FR-002**: TDR recovery resets only the timed-out or faulted engine;
  `QueryEngineStatus` reports host progress.
- **FR-003**: After any transition, guest state is resynchronized with the
  device; a context that cannot be restored is reported lost.
- **FR-004**: After unload, no driver pool allocation and no device-side
  context or blob remains.
- **FR-005**: A device that changed across hibernation is detected and handled
  without a bugcheck.
- **FR-006**: The test executor can be told to stall or fault one context, from
  the in-guest runner, without a rebuild.
- **FR-007**: Every failure is logged once with the value that caused it.

## Success Criteria *(mandatory)*

- **SC-001**: S4 resume followed by a passing `rvktest.exe`, five of five; S3
  likewise where offered.
- **SC-002**: 50 disable and enable cycles and 5 driver upgrades without reboot,
  no leak, no bugcheck.
- **SC-003**: An injected 5 s stall recovers within 10 s with the other
  context's work unaffected.
- **SC-004**: An injected fault loses only the faulted context.

## Assumptions

- The runner re-enables hibernation inside the overlay for the S4 scenario; the
  prepared image keeps it off.
- The echo executor gains stall and fault controls for tests; the real executor
  (042) is not needed.

## Out of Scope

- Live migration and dirty tracking (deferred by 025).
- Hot-unplug of the PCI device from a running guest.
- Restarting QEMU under a running guest.
