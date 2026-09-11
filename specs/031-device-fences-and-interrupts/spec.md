# Feature Specification: QEMU device: fence pages and interrupt injection

**Feature Branch**: `031-device-fences-and-interrupts`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 031 · **Status**: `implement` · **Depends on**: 030

Fence page layout and monotonicity, per-context fence rings, MSI-X injected
from a worker thread through a KVM irqfd with vector masking honoured.

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
| `FenceWindow` | object | a `BlobWindow` over the fence blob | Publishes one context's fence page into its pinned blob |
| `FenceReporter` | object | the fence window, the context's interrupt line, completed fence, progress, prepared and pending flags | Reports one context's completed batches to the guest |
| `FenceInterrupts` | system | host, mutex, whether the guest notifiers are connected, interrupts raised | Raises fence interrupts from workers only while the guest notifiers exist |
| `RingConsumer` (extended) | object | + the context's `FenceReporter` | Consumes one context's complete batches on its worker |
| `ContextBinding` (extended) | object | - | + the bound fence blob's window |
| `DeviceContext` (extended) | object | + `FenceReporter` | One guest API context on the device |
| `DeviceContextTable` (extended) | system | + `FenceInterrupts&` handed to each context | The device's contexts |
| `VirtioGpuRvk` (extended) | object | + `FenceInterrupts` | One virtio-gpu-rvk device instance; now told the driver's status |
| `Host` / `TableHost` / `MockHost` (extended) | contract / object / object | + fence notifier state | + connect and disconnect the fence queues' guest notifiers, raise one |

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

- Q (agent, verified): How does a worker raise an MSI-X interrupt without the
  main loop? -> A: Through virtio-pci's guest notifiers
  (`docs/kb/11-implementation-plan.md`, Threading): the device adds one "fence
  queue" per doorbell page after the control and cursor queues (queues 2..65),
  connects the guest notifiers of every queue with
  `VirtioBusClass::set_guest_notifiers` once the driver sets `DRIVER_OK`, and a
  worker sets the fence queue's notifier. With KVM that notifier is an irqfd
  on the queue's MSI-X vector and virtio-pci handles masking
  (`virtio_pci_vector_mask`/`poll` set the pending bit); without KVM
  virtio-pci's handler raises the vector on the main loop, so qtest sees the
  pending bit of a masked vector and the ISR bit with MSI-X off. The queues
  carry no descriptors: a fence queue is a vector the driver assigns.
- Q (agent, verified): When are the notifiers released? -> A: In the device
  reset, after every context (and so every worker) is gone. virtio-pci writes
  status 0 through `virtio_set_status` before it resets the device, so a status
  without `DRIVER_OK` does not release them; releasing inside `vdc->reset` is
  before `msix_unuse_all_vectors`, as vhost's stop is. Unrealize resets the core
  first.
- Q (agent): A worker racing the notifiers' connection? -> A: `FenceInterrupts`
  holds a host mutex around connect, disconnect and every raise, so a worker
  never touches a notifier that is being set up or torn down. A worker taking a
  mutex is fine on the device side; constitution VII's rule is the guest's
  submission path.
- Q (agent): What does progress count? -> A: Batches completed. It is the TDR
  heartbeat: 032 executes a batch before it completes, and a long execution
  that should keep the heartbeat moving extends this.
- Q (agent): When is the fence page written? -> A: At setup, as completed 0 and
  progress 0, before the ring is attached; then by the worker after every
  completed batch, always from the device's own counters, so a page the guest
  scribbles on is simply overwritten. One interrupt per service that completed
  anything, after the page is written.
- Q (agent): Before `DRIVER_OK`? -> A: Pages are written, no interrupt is
  raised.
- Q (agent): MSI-X vectors? -> A: The PCI function keeps virtio-gpu's
  `vectors` property. A driver that wants a vector per context asks for more;
  sharing one vector across fence queues is valid, and a queue left at
  `VIRTIO_MSI_NO_VECTOR` raises nothing.

## User Scenarios & Testing *(mandatory)*

<!-- Written by /speckit-specify or by hand. The autogen block above is regenerated by tools/gen_specs.py; everything below is preserved. -->

Users: the guest driver's interrupt path (M3's ISR reading fence pages and
reporting `DXGK_INTERRUPT_DMA_COMPLETED`) and, from M6, native fences read by a
user-mode driver. The device writes each context's fence page from its worker
as batches complete and interrupts the guest on the context's own fence queue.

### User Story 1 - Completed batches reach the fence page (Priority: P1)

**Independent Test**: over qtest, bind a context, write batches, store to the
doorbell, and read the fence blob until `completed_fence` is the last batch's.

**Acceptance Scenarios**:

1. **Given** a context just set up, **Then** its fence page reads magic `RVKF`,
   flags 0, completed 0, progress 0, fault code 0.
2. **Given** batches with fences 1..n consumed, **Then** the page reads
   completed n and progress n.
3. **Given** the guest overwrites the page, **When** the next batch completes,
   **Then** the page again holds the device's values, never smaller than before.
4. **Given** a context whose ring stopped (030), **Then** its page no longer
   changes.

### User Story 2 - The guest is interrupted on the context's fence queue (Priority: P1)

**Acceptance Scenarios**:

1. **Given** MSI-X enabled, the driver `DRIVER_OK`, and fence queue `2 + d`
   assigned vector *v* masked, **When** a batch on doorbell *d* completes,
   **Then** vector *v*'s pending bit is set; unmasking delivers it and clears it.
2. **Given** two contexts on vectors *v* and *w*, **Then** a completion on one
   pends only its own vector.
3. **Given** MSI-X disabled, **Then** a completion sets the ISR's queue bit.
4. **Given** a fence queue with no vector, or a driver not yet `DRIVER_OK`,
   **Then** nothing is raised and the page is still written.
5. **Given** a reset, **Then** every context is destroyed before the notifiers
   are released, and after the driver starts again completions interrupt again.

### Edge Cases

- A service that completes nothing raises nothing.
- Several batches in one service: one interrupt, after the last page write.
- A worker raising while the main loop connects or disconnects: serialized.
- The queues' guest kicks: ignored.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: `virtio-gpu-rvk-pci` exposes 66 virtqueues: control, cursor, and a
  fence queue per doorbell page, in doorbell order.
- **FR-002**: The fence page is written only from device state: at setup, and
  after each completed batch.
- **FR-003**: Interrupts are raised from the worker through the fence queue's
  guest notifier, never by queueing work on the main loop.
- **FR-004**: No worker touches a guest notifier while it is not connected.
- **FR-005**: `docs/protocol.md` documents the fence queues and when the page
  is written and the interrupt raised.

## Success Criteria *(mandatory)*

- **SC-001**: The qtest suite reads fence pages and MSI-X pending bits and the
  ISR on the real device for every acceptance scenario.
- **SC-002**: 100% branch coverage of the new core, including every mock-host
  failure of connecting the notifiers.

## Assumptions

- The device's worker is the only writer of the page after setup.
- Faults (flags, fault code) are 033's; this feature always writes a healthy page.

## Out of Scope

- Executing payloads before completion (032); faults and TDR (033).
- The guest's ISR and `FenceProgress` in the driver (034, M3).
