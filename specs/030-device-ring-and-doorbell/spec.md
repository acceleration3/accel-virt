# Feature Specification: QEMU device: submission ring and doorbell

**Feature Branch**: `030-device-ring-and-doorbell`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 030 · **Status**: `implement` · **Depends on**: 029

The hot path, deliberately outside the virtio spec: a per-context ring in
guest memory and a doorbell register backed by a KVM ioeventfd.

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
| `ContextSetup` (extended) | object | + doorbell index | Encodes one context setup, now naming its doorbell |
| `BatchHeader` | object | payload length, fence id | Encodes the 16-byte header that frames one batch of ring bytes |
| `BlobWindow` | object | one blob's extents (pointer, length) and total length | Copies bytes in and out of one pinned blob's memory, bounded by its length |
| `RingWindow` | object | a `BlobWindow`, the ring order | Reads one ring's header and wrapped data bytes, and publishes its head |
| `RingConsumer` | object | the ring window, `RingProgress`, head, last fence id, batches consumed, attached and stopped flags | Consumes one context's complete batches on its worker |
| `Doorbell` | object | host, event, index, connected flag | Connects one context's wake event to its doorbell page |
| `Worker` (extended) | object | + `Doorbell`, the context's `RingConsumer` | Runs one context's posted work and ring service on its thread |
| `WorkItem` (extended) | object | completion now optional | One unit of a context's work |
| `ContextBinding` (extended) | object | + doorbell index | Records what one context's setup bound it to |
| `DeviceContext` (extended) | object | + `RingConsumer`; members ordered so the worker stops before the binding unpins | One guest API context on the device |
| `DeviceContextTable` (extended) | system | - | + whether a doorbell index is held |
| `BlobTable` (extended) | system | - | + a pinned blob's window |
| `Host` / `TableHost` / `MockHost` (extended) | contract / object / object | + doorbell connections | + connect and disconnect a doorbell |

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
- [`docs/kb/11-implementation-plan.md`](../../docs/kb/11-implementation-plan.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11

- Q (agent, verified): Can a doorbell be tested without KVM? -> A: Yes. QEMU's
  `memory_region_dispatch_write` sets a region's matching ioeventfd itself when
  KVM is not enabled (system/memory.c), so a qtest `writel` to the page wakes
  the worker through the same eventfd KVM would signal. The match is on address
  and width: only a 4-byte store at offset 0 of the page.
- Q (agent, verified): Which BAR? -> A: BAR 0. virtio-gpu-pci with `hostmem`
  uses BAR 1 (MSI-X), 2-3 (virtio registers, 64-bit) and 4-5 (hostmem); BAR 0
  is the legacy I/O BAR, unused because the device forces virtio 1. A 32-bit
  non-prefetchable 256 KiB memory BAR, one 4 KiB page per context so a page can
  later be mapped into one process (M6). A virtio shared memory capability
  with id 0x72 locates it, so the driver's existing capability walk finds it.
- Q (agent): How does the worker wake for both posted work and a doorbell? ->
  A: The worker's one wake event becomes the doorbell's eventfd: the host's
  events are eventfds, and connecting a doorbell registers the context's wake
  event as the page's ioeventfd. One wait, no second thread.
- Q (agent): How does the device learn fence ids? -> A: From the ring. Each
  batch starts with a header carrying its fence id, strictly increasing per
  context. The producer assigns them; the guest driver maps dxgkrnl's
  submission fence ids to its own ring fences, which user-mode submission
  (M6) needs anyway. Batches are rounded to 8 bytes.
- Q (agent): Who picks the doorbell index? -> A: The guest, in the setup (at
  offset 24, formerly reserved): the driver must know the page before it can
  map it for a process. Protocol 1.0 has no released peer, so the reserved
  field is taken without a minor bump.
- Q (agent): What happens on a ring violation before faults exist (033)? -> A:
  The context stops consuming and logs once; 033 turns the stop into a fault
  reported in the fence page.
- Q (agent): Is the payload copied? -> A: Not in this feature: nothing reads it.
  Execution (032) copies each batch once before decoding.

## User Scenarios & Testing *(mandatory)*

<!-- Written by /speckit-specify or by hand. The autogen block above is regenerated by tools/gen_specs.py; everything below is preserved. -->

Users: the guest driver's submission path (M3's `SubmitCommandVirtual`) and,
from M6, a user-mode driver storing to its own doorbell. A context set up with a
ring blob and a doorbell index (029) gets its ring serviced by its own worker
thread whenever the guest stores to the context's doorbell page. The device reads
only what it can validate, copies what it reads, and publishes how far it has
consumed; it never trusts the guest's counters beyond what it has already seen.
Nothing is executed and no fence completes yet (031, 032).

### User Story 1 - Ringing a doorbell consumes the ring (Priority: P1)

A guest writes batches into a bound context's ring, advances the ring's tail and
stores to the context's doorbell page. The device consumes every complete batch
and publishes the new head.

**Independent Test**: over qtest, create a context, a guest-backed ring blob and
a fence blob, set it up with doorbell 3, write two batches, store to doorbell
page 3 and poll the ring header until head equals tail.

**Acceptance Scenarios**:

1. **Given** a bound context with doorbell index *d*, **When** the guest writes
   complete batches up to tail *t* and stores any 32-bit value at offset 0 of
   doorbell page *d*, **Then** the ring header's head becomes *t*.
2. **Given** a batch whose bytes are only partly written (tail inside it),
   **When** the doorbell rings, **Then** head stops at the batch's start, and the
   batch is consumed once the rest is written and the doorbell rings again.
3. **Given** a ring whose data wraps past the end of the data area, **When** the
   doorbell rings, **Then** the batch is read across the wrap and head advances.
4. **Given** a store to a doorbell page no context is bound to, to another
   offset of a page, or of another width, **Then** nothing is consumed anywhere.
5. **Given** two contexts with different doorbells, **When** one doorbell rings,
   **Then** only that context's ring is consumed.

### User Story 2 - A hostile ring stops only its own context (Priority: P1)

The guest owns the ring's bytes and can rewrite them at any time. Anything no
valid producer can write stops that context's consumption; the device stays up.

**Acceptance Scenarios**:

1. **Given** a bound context, **When** the ring header's tail moves backwards,
   exceeds head by more than the data area, its magic or order change, or a
   batch has a bad magic, a length that cannot fit the data area, fence id 0 or a
   fence id not greater than the last consumed, **Then** the device consumes
   nothing more on that context, logs the reason once, and later doorbells for
   it are ignored.
2. **Given** a stopped context and a healthy one, **Then** the healthy one keeps
   consuming, and CTX_DESTROY of the stopped one succeeds.

### User Story 3 - The setup names the doorbell (Priority: P1)

**Acceptance Scenarios**:

1. **Given** a context setup naming doorbell *d* below the capset's context
   count, a ring blob whose header carries the setup's order and head 0, **Then**
   the setup succeeds and doorbell page *d* is connected to that context.
2. **Given** a setup naming a doorbell at or above the context count, a doorbell
   another context holds, non-zero reserved bytes, or a ring header that is
   malformed, has another order, or head other than 0, **Then** it is refused
   with `ERR_INVALID_PARAMETER` and nothing is connected or pinned.
3. **Given** a bound context, **When** it is destroyed or the device resets,
   **Then** its doorbell is disconnected before its worker stops and before its
   blobs are unpinned, and the index can be set up again.

### Edge Cases

- A doorbell rung before any data is written, or rung repeatedly: no change.
- Tail exactly `head + 2^order` (full ring): consumed.
- A zero-length batch (header only): consumed; it is how a guest pings.
- Head written by the guest: ignored; the device's own counter is published.
- A doorbell store racing CTX_DESTROY: the context's worker is joined after
  disconnect, so no ring access outlives the pinned blobs.
- A worker woken by posted work and a doorbell together: both are served.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The device exposes one doorbell page per context the capset allows
  (64 pages of 4096 bytes), in PCI BAR 0, located by a virtio shared memory
  capability with id `0x72`.
- **FR-002**: The context setup carries the doorbell index at offset 24 (u32);
  offset 28 stays reserved.
- **FR-003**: A 32-bit store at offset 0 of a connected doorbell page wakes the
  context's worker without the main loop (KVM ioeventfd, or QEMU's dispatch
  without KVM).
- **FR-004**: The ring is consumed as batches: a 16-byte header (`RVKB` magic,
  u32 payload length, u64 fence id) and the payload, together rounded up to a
  multiple of 8 bytes. Every header is copied before it is validated.
- **FR-005**: Every snapshot of the ring header is judged against the device's
  own history (`RingProgress`): tail never moves backwards, never runs more than
  `2^order` ahead of head, order and magic never change.
- **FR-006**: Head is published after every consumed batch.
- **FR-007**: A violation stops the context's consumption permanently and logs
  once; it never asserts, never reads outside the pinned ring, and never affects
  another context.
- **FR-008**: `docs/protocol.md` documents the doorbell region, the setup field,
  the batch layout and the device's consumption rules.

### Key Entities

- **Doorbell page**: per context index; a store is a wake-up, its value unused.
- **Batch**: one producer-framed unit of ring bytes completing one fence.
- **Ring consumption state**: the device's head, the last tail and fence id seen,
  and whether the context has stopped.

## Success Criteria *(mandatory)*

- **SC-001**: The qtest suite rings real doorbells on `virtio-gpu-rvk-pci` and
  observes head advance for complete, partial, wrapped and multi-context rings,
  and stop for each hostile ring.
- **SC-002**: 100% branch coverage of the new core; `fuzz_ring` survives the
  60-second smoke with a committed corpus.
- **SC-003**: A doorbell store reaches the worker with the main loop idle: the
  qtest consumes a batch while no control request is in flight.

## Assumptions

- The producer frames batches and assigns fence ids; for the kernel submission
  path the guest driver maps dxgkrnl's fence ids to its own (M3). The device
  never sees a submission descriptor.
- Rings of guest-backed and host-visible blobs are both accepted; each is read
  through the blob's own memory from the worker.
- The ring header's magic and order are written by the guest before the setup.

## Out of Scope

- Fence pages, completion reporting and interrupts (031).
- Decoding or executing batch payloads (032); faults reported to the guest (033).
- The guest driver's producer side (034 drives the ring from the guest core).
