# Implementation Plan: the guest transport

**Branch**: `043-guest-kmt-transport` | **Date**: 2026-09-14 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/043-guest-kmt-transport/spec.md`

## Summary

The wire is complete, the device runs all three APIs (042), and the guest-side
rvk state machine has existed since 034 - but nothing joins a process to it. A
`D3DKMTCreateContextVirtual` makes a scheduling context and refuses private
data; a submission's DMA buffer is never read; no rvk fence reaches user mode.
This feature is the join: the miniport learns to build an rvk `GuestContext`
from a context's private data, publish its ring, fence page and reply as guest
blobs a process can map, read the submission descriptor a process leaves in the
DMA buffer, ring the doorbell itself, and complete the submission fence when the
device's fence page says the batch finished. On top of that sits the
`ClientTransport` the generated client dispatch is handed.

Three things the research settled and the plan is built on:

1. **The DMA buffer is the opening.** `DXGKARG_SUBMITCOMMANDVIRTUAL` hands the
   driver the guest VA of the buffer; the wire's `SubmissionDescriptor` is what
   goes in it. No escape.
2. **Transport memory is aperture-segment allocations published as guest
   blobs.** VidMm hands the driver those pages, the device already accepts a
   GUEST blob as a ring, and `D3DKMTLock2` maps them to the process. The
   host-visible arena 041 could not place waits on 041.
3. **The generated client is made to compile by changing the emitter to lend
   the writer**, not by growing `wire_*`. `send_*` gains the overflow check the
   client never had.

## Technical Context

**Language/Version**: C++23, freestanding kernel-mode for the miniport additions
and the transport core; the same for the guest suite. Python 3 for the emitter
change and the guest runner.

**Primary Dependencies**: the guest-side rvk classes (034); host-visible and
aperture allocations (041); `ContextLoss` and TDR (040); the generated client
dispatch and the emitter (063); the running device and executor (042) for the L4
and L5 ends.

**Testing**: L1 unit tests over every new class with `MockPlatform`/`MockKmt`;
the L4 loopback rig extended with a `Kmt`-driven path; a fuzz target over the
reply and fence-page readers; and `tools/guest/run_guest_suite.py` driving a
transport round trip in a real guest.

**Target Platform**: the Windows guest (x64), kernel and user mode; the
transport core is portable and tested on Linux.

**Constraints**: no STL, no exceptions, no RTTI, no heap on the submission path;
IRQL typed on every method; everything read from a mapped ring, reply or fence
page is hostile and validated before use (the device and the guest distrust each
other's shared memory equally).

**Scale/Scope**: one transport interface; the miniport gains context private
data, blob publication, DMA-buffer submission, a fence interrupt and a fault
path; the emitter changes for three APIs; Vulkan and OpenCL clients link.

## Constitution Check

| Principle | How this feature satisfies it |
|---|---|
| I. Specification precedes implementation | The spec was corrected in this pass where 024 and 041 turned out not to provide what it assumed; research and this plan precede the code. |
| II. The core is portable; the shim is not | The transport and the readers are portable core; the D3DKMT calls are `Kmt`, already the shim seam, and the miniport DDI glue is the existing one-line boundary. |
| III. Object-oriented, with state | Every class below owns state and names its kind. The `Kmt` and DDI boundaries are the constitution's existing exceptions. |
| IV. Test-first and exhaustive | Each class is tested before it is written; the reply and fence-page readers get a fuzz target and a hostile-mapping suite (SC-002). |
| V. Everything from outside is hostile | A mapped ring, reply and fence page are written by the other side and validated on every read; `FenceObserver` already fails a context on a lying page. |
| VI. Feature levels are phases | Vulkan first over the transport; OpenCL follows; OpenGL is left at a stated contract. Each phase stops cleanly. |
| VII. IRQL and locking are typed | Context creation is PASSIVE; submission and service are DISPATCH; the fence interrupt is at the ISR's IRQL. `GuestContext` already carries these annotations. |
| VIII. No speculative structure | The host-visible arena is not built because 041 cannot place it; a reply is copied because the BAR is not mapped to the process; the OpenGL client is not linked because 050's helpers do not exist. Each absence is a missing caller, not a choice. |
| IX. Reproducible and observable | The L4 round trip is deterministic; a fault names its code in the fence page and the log; the guest run is the L5 reproduction. |

No violations. The one thing that could look like a violation - reusing the
scheduling context's submission fence rather than adding a native-fence DDI - is
in the table above: a new DDI slot with one implementation is the speculative
structure the constitution forbids, and the existing fence, signalled from the
page, carries the same information.

## Classes this feature introduces

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `GuestConnection` | system | one adapter and device, and the per-sub-API transports over them | Hold the one link a process shares across its rvk contexts |
| `ClientTransport` | object (interface) | none; two real implementers | Carry one context's commands to the device and answers back |
| `KmtTransport` | object | the `Kmt`, the device and context handles, the mapped ring, reply and fence views | Be the transport a process holds over D3DKMT |
| `TransportRing` | object | the mapped ring block and the tail it has published | Put one batch in the ring |
| `ReplyReader` | object | the mapped reply block | Read one answer, refusing a slot no device wrote |
| `TransportFence` | object | the fence ids this transport has submitted and seen | Say which submission has completed, from the fence page |
| `RvkContextRecord` | object | one context's private data, decoded | Say what host context a `CreateContext` asked for |
| `RvkContextBinding` | object (KMD) | the `GuestContext` a dxgkrnl context owns, and its three blobs | Bind one dxgkrnl context to an rvk context |
| `ContextPages` | object (KMD) | the page lists and kernel views of a context's ring, fence and reply | Describe the memory a context runs on; the sequence publishes the blobs over it, under ids the binding assigns |
| `SubmissionReader` | object (KMD) | the platform it copies a guest buffer through before trusting it | Turn a submitted DMA buffer into an RVKS the ring submitter can act on |
| `FenceInterruptRoute` | system (KMD) | which context each fence queue vector serves | Wake the context whose fence page the device just wrote |

Eleven. Five are what a process holds - `GuestConnection`, the shared link
US1 requires, and the `ClientTransport` (with `KmtTransport`, `TransportRing`,
`ReplyReader`, `TransportFence`) it hands out one of per sub-API; five are the
miniport learning to serve an rvk context; `RvkContextRecord` is the record that
crosses the private-data boundary and is shared by both sides. `GuestConnection`
is why US1's "one link shared by all three APIs" has an owner rather than being
smeared across three `KmtTransport`s.

`GuestContext`, `RingSubmitter`, `FenceObserver`, `ContextRequestSequence` and
`ContextMemory` are **not** in this table: they exist (034) and this feature
gives them their first production caller rather than rewriting them.
`ClientBlobArena` is named in the generated client and is 043's to define, but
it is one class (`KmtBlobArena`) that belongs to the transport group above and
is added when the OpenGL write path needs it; it is listed as a follow-task
rather than built before a caller, per principle VIII.

## Project Structure

```text
src/proto/client_transport.hpp            # the interface the generated client is handed
src/guest/transport/                      # GuestConnection, KmtTransport, TransportRing, ReplyReader, TransportFence
src/guest/kmt/kmt.hpp                      # extended: create-context private data, Lock2-shaped map, CPU fence wait
src/proto/context/rvk_context_record.*    # the private-data record, both sides
src/core/rvk/                             # RvkContextBinding, ContextPages, FenceInterruptRoute
src/core/adapter/                         # CreateContext private data, SubmissionReader wired in
tools/codegen/rvk_codegen/emit/           # the writer-lending change, three APIs
tests/loopback/                           # the rig gains a Kmt-driven round trip
tests/fuzz/fuzz_reply_reader.cpp          # the reply and fence-page readers over hostile bytes
```

**Structure Decision**: the transport a process holds lives in `src/guest/`
beside `Kmt`; the miniport's half lives in `src/core/rvk/` and `src/core/adapter/`
beside the classes it drives; the record that crosses between them is in
`src/proto/` because both compile it.

## Phase 0: research

`research.md` answers the three questions and records every decision with the
alternative rejected: private data over an escape, aperture blobs over
host-visible or kernel-mapped memory, the emitter lending the writer over a new
growable type, and the submission fence over a new fence DDI.

## Phase 1: design

`data-model.md` describes the transport's data - the record, the ring's
counters, the reply and fence layouts as the guest reads them - and the
lifetime rules that keep a mapped view valid. `contracts/client_transport.md`
states the interface the generated client is handed, member by member, and what
`send_*` guarantees about a writer that overflowed. `quickstart.md` is how to
build the transport, drive it over L4, and run the guest round trip.

## Complexity Tracking

| Violation | Why needed | Simpler alternative rejected because |
|---|---|---|
| Ten classes | A transport, its four moving parts, and five miniport pieces that had no user-mode path before | Fewer would put the ring, the reply and the fence in one object with three lifetimes and three writers |
| `ClientTransport` interface with (initially) one real implementer | The generated client is handed it and the loopback rig substitutes a second | Not speculative: the L4 rig's transport and `KmtTransport` are two implementations from the first test, the way `Platform` has two |
| Extending the `Kmt` seam (private data on create-context, a lock that returns a view, a CPU wait on a fence) | The transport needs D3DKMT calls the suite never made; the seam is where the shim boundary already is | Adding them elsewhere would put D3DKMT calls outside the one seam the constitution's portability rule draws |
