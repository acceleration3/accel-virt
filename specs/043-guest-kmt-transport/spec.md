# Feature Specification: Guest transport: render-server's drivers over D3DKMT and the rvk ring

**Feature Branch**: `043-guest-kmt-transport`

**Created**: 2026-09-11

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 043 · **Status**: `implement` · **Depends on**: 041, 024, 034, 063

The guest half of the seam: a user-mode transport that gives render-server's
Vulkan, OpenGL and OpenCL drivers everything its shared-memory link gives them
today - a per-process link shared by all three APIs, synchronous and
asynchronous commands, command streams, a reply area, a blob arena of host-
visible memory, fences and completed sequence numbers - built from an rvk
context per sub-API, its ring and doorbell, fence waits that sleep instead of
polling, and allocations from feature 041. Portable logic tested on Linux over
MockKmt and the loopback rig.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

### Note

render-server's link holds its ring transport concretely; the contract this
feature needs from render-server is a transport it can be handed, stated in
the spec.

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
| | object / system | | |

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
- [`docs/kb/07-usermode.md`](../../docs/kb/07-usermode.md)
- [`docs/kb/11-implementation-plan.md`](../../docs/kb/11-implementation-plan.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## User Scenarios & Testing *(mandatory)*

Users: render-server's Windows drivers - the Vulkan ICD, the OpenCL ICD and the
OpenGL driver - which today reach their host through a shared-memory region
(ivshmem or a shared file) with a polling doorbell. Inside a guest with rvkgpu
they must instead reach the host through the adapter: D3DKMT for setup, an rvk
context's ring and doorbell for commands, rvk fences for completion.

### User Story 1 - render-server's drivers run over rvkgpu unchanged above the link (Priority: P1)

**Independent Test**: on Linux, the transport's portable core runs over `MockKmt`
and the L4 loopback rig with the device core and the echo executor; a scripted
client issues synchronous, asynchronous, batched and stream commands and reads
back replies and sequence numbers exactly as it would over the shared-memory
link.

**Acceptance Scenarios**:

1. **Given** a process in which the first render-server driver loads, **When** it
   asks for its link, **Then** the transport opens the rvkgpu adapter by LUID,
   creates a device, and one rvk context per sub-API the process uses; a second
   driver in the same process shares the same link.
2. **Given** a link, **When** a driver sends a synchronous command, **Then** it is
   written to the context's ring, the doorbell is rung, and the call returns
   with the reply the executor wrote to the context's reply area.
3. **Given** asynchronous commands, batches and command streams, **When** they are
   sent, **Then** they are ordered on the ring, `last_published_seqno` and
   `seqno_reached` report the published and completed sequence numbers, and the
   completed number comes from the context's fence page.
4. **Given** a driver waiting for a sequence number or a fence, **When** it is not
   yet reached, **Then** the thread sleeps on a monitored fence wait
   (`D3DKMTWaitForSynchronizationObjectFromCpu`) and wakes on the interrupt - it
   never polls.

### User Story 2 - A blob arena of host-visible memory (Priority: P1)

**Independent Test**: over the loopback rig, the transport allocates blobs, maps
them, and the device's view of a written byte matches; a 32-bit view is checked
to lie below 4 GiB.

**Acceptance Scenarios**:

1. **Given** a driver asking for blob memory, **When** the transport allocates
   it, **Then** it is an allocation from feature 041 mapped into the process, and
   the blob import render-server's drivers request (`kProtoQueryBlobImport`,
   `kProtoImportBlob`) resolves to that allocation's blob on the host.
2. **Given** a 32-bit process, **When** a low view is requested, **Then** it lies
   below 4 GiB or the allocation fails with a status.
3. **Given** retired blobs, **When** their fences complete, **Then** the transport
   releases the allocations.

### User Story 3 - Faults, TDR and device loss reach the driver (Priority: P1)

**Independent Test**: `MockKmt` scripts a faulted fence page, a device-lost status
from every D3DKMT call, and a doorbell disconnect; the transport reports each
through `connected()` and failing sends.

**Acceptance Scenarios**:

1. **Given** a context the device faults, **When** the driver next sends, **Then**
   the send fails, `connected()` is false, and the driver can report device lost.
2. **Given** a TDR on the adapter, **When** the driver waits, **Then** the wait
   returns, and a new link can be created once the adapter recovers.

### Edge Cases

- The ring is full: a send waits on the fence of the oldest unconsumed record,
  bounded by a timeout, never overwrites.
- A reply larger than `reply_capacity()`: the send fails with a status; chunked
  replies follow render-server's partial-reply rule.
- The fence page or reply area holds hostile values (offsets past the area,
  sequence numbers that go backwards, a reply length beyond capacity): the
  transport treats the context as faulted, never reads outside its mapping.
- The adapter is removed or disabled while a process holds a link: every call
  fails with device lost; nothing touches unmapped memory.
- The doorbell is disconnected (suspend, TDR): stores land on a harmless page and
  the transport falls back to `D3DKMTSubmitCommand` until reconnected.
- Several threads in one process load the drivers at once: exactly one link is
  created and every driver shares it.
- Budget exceeded when allocating blobs: allocation fails with an out-of-memory
  status the driver maps to its API's error.

## Requirements *(mandatory)*

- **FR-001**: The transport implements render-server's abstract `Transport`
  (`send_sync`, `send_async`, `send_async_batch`, `reply_capacity`, `connected`,
  `name`) and the stream, sequence-number and blob operations render-server's
  drivers use today (`send_stream`, `seqno_reached`, `last_published_seqno`,
  blob arena, retired blobs, low views).
- **FR-002**: render-server must let its per-process link be constructed over a
  transport handed to it, instead of holding its ring transport and shared region
  concretely, and must not read the shared-region header directly; its fences
  (`RegionHeader.fences`), completed sequence number and `gl_error` word become
  operations on the transport.
- **FR-003**: Portable decision logic lives in this repository's guest code, is
  tested on Linux over `MockKmt` and the loopback rig to constitution IV, and a
  Windows shim translates to gdi32 only.
- **FR-004**: One rvk context per sub-API per process; the context's capset
  sub-API is Vulkan, OpenGL or OpenCL as the driver requests.
- **FR-005**: Every value read from a mapped area is validated before use; a
  violation faults the link.
- **FR-006**: Waiting uses rvk fences through D3DKMT; no busy polling, no
  fixed-period parking.
- **FR-007**: Setup uses `D3DKMTSubmitCommand` doorbell-less submission; feature
  049 moves the hot path to WDDM doorbells.
- **FR-008**: The present area of render-server's region is not reproduced;
  presentation goes through the KMD (046, 051).

## Success Criteria *(mandatory)*

- **SC-001**: render-server's transport round-trip, chunked-command, partial-reply,
  deep-nesting, wire-fault and ring-edge tests pass against this transport over
  the loopback rig.
- **SC-002**: 100% branch coverage of the transport core; a hostile-mapping table
  test and a fuzz target for the reply and fence-page readers.
- **SC-003**: No thread in a waiting process consumes CPU while waiting.

## Assumptions

- render-server accepts the link change in FR-002; until then the transport is
  exercised by this repository's test client only.
- Features 024 and 041 provide mappable rings, fence pages and host-visible
  allocations.

## Out of Scope

- The drivers' API coverage, which stays render-server's.
- Doorbells and native fences on the hot path (049).
- Cross-process sharing (053).
