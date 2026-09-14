# Feature Specification: QEMU device: fault, reset and recovery

**Feature Branch**: `033-device-fault-and-reset`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 033 · **Status**: `implement` · **Depends on**: 032

A faulted context is torn down without disturbing others, reported to the
guest, and reconciled with the guest's TDR.

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
| `FaultCode` | enum | - | The fence page's fault codes, shared by both sides |
| `FenceReporter` (extended) | object | + faulted flag, fault code | Reports one context's completions, heartbeats and fault to the guest |
| `BatchExecutor` (extended) | object | + the context's `FenceReporter`, the last failure's fault code | Executes one context's batches, moving progress per command |
| `RingConsumer` (extended) | object | - | Consumes one context's batches; a stop is now a fault that destroys the executor |
| `MockContextExecution` (harness, extended) | object | + reporter it is built on | Execution stack for tests above it |

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

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11

- Q (agent): What is a fault? -> A: Every reason 030 and 032 stop a context:
  a ring header that is malformed or changes order, a tail inconsistent with
  what was consumed, a batch header that is malformed, cannot fit the ring or
  does not follow the last fence, commands framed wrong or asking for an answer
  with no reply slot, a payload the executor cannot decode, a lost host device,
  and an executor that could not be created. Each has its own `fault_code` in
  the fence page, so the guest's TDR report can say which.
- Q (agent): What does "torn down" mean on the device? -> A: The moment it
  faults, the worker destroys the context's host executor - the host API's
  objects are freed without waiting for the guest - and consumes nothing more.
  The context itself, its binding, doorbell and pins stay until `CTX_DESTROY`
  or a reset, because the guest owns those resources and must see the fault
  before it lets go of them.
- Q (agent): How is the guest told? -> A: The fence page is rewritten with
  `faulted`, the last completed fence, the current progress and the code, and
  the context's fence queue interrupts, exactly as a completion does. Once
  written the flag never clears for the context's lifetime.
- Q (agent, verified): What does the guest's TDR need from the device? -> A:
  From `docs/kb/05-scheduling.md`: `QueryEngineStatus` must reflect host
  progress, preemption at worst stops feeding the ring, and a reset must
  resynchronize guest and host state. So: progress moves for every command
  executed, not only per batch, so a long batch is not a hang; a context the
  guest resets (`CTX_DESTROY`, then create and set up again, with the same ids,
  blobs and doorbell) is independent of the one it replaces; and a device reset
  (the adapter-wide `ResetFromTimeout`) destroys everything and accepts a new
  driver's setup. Preemption needs nothing new: the guest stops publishing tail.
- Q (agent): A context whose host call never returns? -> A: Out of scope. A
  worker in a host API call cannot be interrupted, and `CTX_DESTROY` and reset
  join it; an executor library is required to return (the ABI says so). The
  heartbeat is what lets the guest tell such a context from a slow one.

## User Scenarios & Testing *(mandatory)*

<!-- Written by /speckit-specify or by hand. The autogen block above is regenerated by tools/gen_specs.py; everything below is preserved. -->

Users: the guest driver's interrupt and TDR paths (M3): an ISR that reads a
faulted fence page and reports `DXGK_INTERRUPT_DMA_FAULTED`, a
`QueryEngineStatus` that watches progress, and a reset that recreates contexts.

> **Superseded (044):** the driver never reports `DMA_FAULTED` - dxgmms2.sys `VidSchDdiNotifyInterruptWorker` @140031b80 bugchecks 0x119/0xD on `DXGK_INTERRUPT_DMA_FAULTED` from a GPU MMU adapter; a faulted context's queued work is completed, its later submissions are refused, ResetEngine reports nothing more, and `ContextLoss` is deleted (`docs/kb/05-scheduling.md`).

### User Story 1 - A fault reaches the guest (Priority: P1)

**Independent Test**: over qtest, bind a context, send a batch the echo
executor cannot decode, and read the fence page's flag and code and the
fence queue's pending bit.

**Acceptance Scenarios**:

1. **Given** a context that has completed fence *n*, **When** anything above
   faults it, **Then** its page reads `faulted`, completed *n*, the progress so
   far and that fault's code, and its fence queue interrupts once.
2. **Given** a faulted context, **When** the guest publishes more batches and
   rings, **Then** the page does not change and nothing executes.
3. **Given** a faulted context, **Then** its host executor has already been
   destroyed, on its worker.

### User Story 2 - Faults stay in their context (Priority: P1)

1. **Given** two contexts, **When** one faults, **Then** the other keeps
   executing, completing and interrupting.
2. **Given** a faulted context, **When** the guest destroys it and sets up a new
   context with the same id, blobs and doorbell (a fresh ring header), **Then**
   the new context executes and its page reads healthy from zero.
3. **Given** faulted and healthy contexts, **When** the device resets, **Then**
   everything is released and a new driver's contexts execute.

### User Story 3 - Progress shows a busy context is not hung (Priority: P2)

1. **Given** a batch of *k* executed commands, **Then** progress has moved by
   at least *k* when the batch completes, and the page reads the progress of
   the commands executed so far while it runs.

### Edge Cases

- A fault before any batch (executor creation failed): completed 0.
- A fault in the middle of a batch: the batch does not complete; progress
  counts the commands that ran.
- Two faults cannot happen: the first stops the context.

## Requirements *(mandatory)*

- **FR-001**: Fault codes (u32): 1 ring header, 2 ring tail, 3 batch header,
  4 batch order, 5 command framing, 6 undecodable, 7 device lost,
  8 no executor. 0 never appears with `faulted`.
- **FR-002**: The faulted page and the interrupt come from the worker, after the
  executor is destroyed.
- **FR-003**: `docs/protocol.md` documents the codes, when progress moves, and
  the recovery sequence.

## Success Criteria *(mandatory)*

- **SC-001**: The qtest suite faults contexts for every code it can reach on
  the real device and reads the page, the pending bit and a recovered context.
- **SC-002**: 100% branch coverage of the changed core; `fuzz_ring` and
  `fuzz_command_stream` check faulted pages decode and stay faulted.

## Out of Scope

- Interrupting a host call that never returns.
- The guest driver's TDR (M3; 034 exercises the guest's reading of faults).
