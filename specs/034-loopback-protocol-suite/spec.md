# Feature Specification: Loopback protocol test suite

**Feature Branch**: `034-loopback-protocol-suite`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 034 · **Status**: `implement` · **Depends on**: 030, 031

Guest core and device core in one Linux process over an in-memory transport.
Where protocol design errors get found before either side is written.

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
| `RvkRequestWriter` | object | destination, capacity, last length | Lays out the virtio-gpu requests a driver sends to use rvk contexts |
| `GpuResponseReader` | object | response type, capset info fields, decoded capset | Decodes one control response the driver receives, bounded by its length |
| `CapsetQuery` | object | step, capset info, decoded capset, outcome | Reads the device's rvk capset and judges it against the driver's protocol |
| `ContextMemory` | object | platform, ring/fence/reply blocks, their sizes and physical addresses | Holds one context's guest-backed ring, fence page and reply slot |
| `ContextRequestSequence` | object | ids, setup, memory's addresses, requests sent, answers accepted, refusal | Sends one context's setup or teardown requests in order and accepts their answers |
| `RingSubmitter` | object | ring pointer and order, tail, device head history, doorbell window and page | Publishes one context's submissions into its ring and rings its doorbell |
| `FenceObserver` | object | fence page pointer, the context's fence history | Reads one context's fence page against what the driver submitted |
| `GuestContext` | object | memory, sequence, submitter, observer, fence progress, lock, state | One rvk context as the driver sees it, from setup to teardown |
| `MockHost` (harness, extended) | object | + a guest memory translator | Maps guest pages from another harness's memory |
| `LoopbackDevice` (harness) | object | mock host, mock executor, the device, doorbell pages | The device core as a guest core reaches it in one process |

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

- [`docs/kb/11-implementation-plan.md`](../../docs/kb/11-implementation-plan.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11

- Q (agent, verified): What does the guest core do with the device today? ->
  A: Nothing rvk. The driver's contexts have no device context, submissions
  complete in a loopback engine (`ExecutionNode`), the control queue sends
  fire-and-forget requests of at most 64 bytes and reads 24-byte answers, no
  fence queue has a vector, BAR 0 is never mapped, and allocations have no
  backing the driver can read. Nothing in `src/core` uses `src/proto`'s ring,
  fence or submission structures.
- Q (agent): So what is "guest core" in this suite? -> A: The driver side of
  the rvk protocol, written as core objects a driver composes: reading the
  capset, one context's memory, its setup and teardown requests, publishing a
  submission into its ring and ringing its doorbell, and reading its fence
  page. They are the objects the miniport's DDIs will hold; wiring
  `CreateContext`, `SubmitCommandVirtual`, the fence-queue ISR and the TDR DDIs
  to them - and allocations the device can see - is the driver's M3 work and
  is not part of this feature.
- Q (agent): The transport? -> A: `docs/testing.md` L4: a ring in a shared
  buffer, a doorbell that is a function call, an interrupt that is a callback.
  Control requests pass as bytes to the device core's `VirtioGpuRvk::control`
  and answers come back as bytes; the ring, fence page and reply slot are the
  driver's contiguous blocks, which the device maps through `MockHost` from
  `MockPlatform`'s physical addresses; a doorbell store is a `RegisterWindow`
  write that rings `MockHost`'s doorbell; an interrupt is the test serving the
  context once the device raised its line. The virtqueues are left to L2
  (driver) and qtest (device), which already cover them.
- Q (agent): How are requests and answers paired without a queue? -> A: The
  driver's objects are pull-style: `next_request` writes the next request a
  sequence needs, `accept` takes its answer. The miniport will feed answers
  from its DPC harvest in the order the control queue completes them, which
  is the order sent.
- Q (agent): Where does user mode write batches? -> A: In this suite the test
  writes them into the context's ring block, as a user-mode driver will once
  the ring is mapped for it (M3/M6). The driver object publishes only
  submissions: a `SubmissionDescriptor` whose `begin` is the tail it published
  last.
- Q (agent): Protocol version mismatch? -> A: The driver refuses a capset
  whose major is not its own, and a device refuses a setup of another major;
  both are exercised.

## User Scenarios & Testing *(mandatory)*

<!-- Written by /speckit-specify or by hand. The autogen block above is regenerated by tools/gen_specs.py; everything below is preserved. -->

Users: the guest driver authors, who get the driver side of the protocol as
tested objects, and protocol designers, who see both sides of every rule run
against each other in one process in seconds.

### User Story 1 - A context from capset to completed fence (Priority: P1)

**Independent Test**: in one process, a guest context reads the capset, sets
up on the device core, the test writes batches, the context publishes and rings,
the device's echo commands run, and the context reads completions and replies.

**Acceptance Scenarios**:

1. **Given** a device with an executor, **When** the driver queries the capset,
   **Then** it decodes and is compatible; **given** a device answering a capset
   of another major, **Then** the driver refuses it; **given** a setup of
   another major, **Then** the device refuses it.
2. **Given** a compatible capset, **When** a guest context runs its setup
   sequence, **Then** every answer is OK and the device holds a bound context
   on the driver's ring, fence page, reply slot and doorbell.
3. **Given** an established context, **When** batches are written and their
   submissions published, **Then** the doorbell reaches the device, the batches
   execute, the fence line is raised, and serving the context reports the last
   fence completed and the reply written.
4. **Given** a context, **When** it is torn down, **Then** its teardown
   sequence is answered OK and the device holds neither context nor blobs.

### User Story 2 - Backpressure and recovery (Priority: P1)

1. **Given** a ring whose unconsumed bytes would pass its capacity, **When** a
   submission is published, **Then** it is refused with no_space and nothing is
   written; once the device's head moves, it is accepted.
2. **Given** a faulted context and a healthy one, **Then** the faulted one
   reports its code, the healthy one completes, and the faulted one can be torn
   down and set up again.
3. **Given** a device reset, **Then** a context's next serve finds nothing
   more, and a new context sets up.

### User Story 3 - Each side survives the other (Priority: P1)

1. **Given** a hostile guest (malformed batches, a descriptor that skips or
   rewinds the tail, fences out of order), **Then** the driver refuses what it
   can see and the device faults the rest.
2. **Given** a hostile device (refused or malformed answers, a capset that does
   not decode, a fence page that moves backwards, completes an unsubmitted fence
   or clears a fault, a head that passes the tail), **Then** the driver's
   objects return errors, write nothing outside their blocks, and a context
   that saw a lie is failed.

### Edge Cases

- An answer shorter than a header, or of an unexpected type, at every step.
- Teardown of a context whose setup failed part way: only what was created is
  released.
- A submission before setup completes: refused.

## Requirements *(mandatory)*

- **FR-001**: Every driver object declares its IRQL ceiling: requests and answers
  at most DISPATCH (the DPC), submission at DISPATCH, observation at device
  IRQL, memory at PASSIVE.
- **FR-002**: Nothing the device writes (answers, head, fence page) is trusted:
  each is copied and judged against the driver's own history.
- **FR-003**: The loopback suite runs in `ctest` in the default, coverage and
  fuzz configurations.

## Success Criteria *(mandatory)*

- **SC-001**: `tests/loopback/` covers every acceptance scenario with the real
  device core and executor mock.
- **SC-002**: 100% branch coverage of the new core; `fuzz_gpu_response` and
  `fuzz_guest_context` clean.

## Out of Scope

- The miniport's DDIs using these objects, fence-queue vectors and BAR 0
  mapping in the driver, allocation backing and GPU VA, shared allocations (M3).
- Virtqueue emulation in the loopback (covered by L2 and qtest).
