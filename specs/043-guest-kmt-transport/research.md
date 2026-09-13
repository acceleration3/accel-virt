# Research: the guest transport

Three questions had to be answered from the code before this could be planned:
what the guest side already has, what the generated client dispatch demands of a
transport, and what the kernel driver and the wire actually provide for a round
trip. All three were read from the tree, and the load-bearing kernel behaviour
was confirmed in Ghidra rather than assumed.

The headline, which the spec did not have when it was written: **there is no
user-mode path to a host rvk context at all.** The wire and the device side are
complete and tested; the guest-side rvk state machine (`GuestContext`,
`RingSubmitter`, `FenceObserver`, `ContextRequestSequence`) exists and is
exercised only in the L4 loopback rig; and nothing in the miniport connects the
two. So 043 is not "a transport over plumbing 024 and 041 laid" - it is the
plumbing, and then the transport.

## 1. What the spec assumed that is not true

- **"024 provides mappable rings and native fences."** 024 declared the WDDM
  doorbell and native-fence DDI slots and left all nine null on purpose. Nothing
  registers them.
- **"041 provides host-visible allocations a process can map."** 041 parked
  exactly that: VidMm refuses every allocation whose preferred segment is the
  CPU-visible one (`0xC000000D` from dxgkrnl after the driver returned success),
  across three live experiments, and pinning was deferred to this feature by
  name. Every allocation today lands in VidMm's implicit system segment, whose
  guest pages the driver never learns unless the allocation gets a GPU VA.
- **A context setup names its ring, fence and reply by blob id with no offset,
  and the three ids must differ** (`docs/protocol.md`, RVKC). The only
  host-visible blob the driver makes is the whole hostmem region (`0x1000`). So
  even with placement working, three rings could not be named inside one region
  blob.

FR-002 already reckons with this - it says the transport is defined here and the
generated client is handed one - but the spec's Assumptions section is wrong on
024 and 041 and is corrected in this pass.

## 2. The opening in the kernel path

`DXGKARG_SUBMITCOMMANDVIRTUAL` carries `DmaBufferVirtualAddress`,
`DmaBufferSize` and `pDmaBufferPrivateData`
(`third_party/windows-kit/include/wdk-shared/d3dkmddi.h:5218`), and
`DxgkSubmitCommandInternal` copies the whole `D3DKMT_SUBMITCOMMAND` from user
space and hands the driver the DMA buffer at the VA it was given (confirmed in
`re/ghidra-proj`, `dxgkrnl.sys` @1402bb890). The current miniport admits the VA
and completes the fence from its own DPC **without reading the buffer**
(`src/core/adapter/adapter.cpp`, `ExecutionNode`). That is the seam: the DMA
buffer is guest memory the driver may read, and the wire's own
`SubmissionDescriptor` (RVKS, 32 bytes) is exactly what belongs in it.

`DXGKARG_CREATECONTEXT` carries `pPrivateDriverData`
(`d3dkmddi.h:1583`); `ContextTable::create` refuses any today
(`src/core/scheduling/context_table.cpp:21`). Accepting it is how a process asks
for an rvk context.

### Decisions

- **A context becomes an rvk context by its private data.** `D3DKMTCreateContextVirtual`
  carries a small record - sub-API, ring order, and which of the context's
  allocations are ring, fence and reply - and the miniport, seeing it, builds a
  `GuestContext` beside the scheduling `Context` and drives its
  `ContextRequestSequence` to the device. No escape: an escape is a second
  entry point with no ordering against submission, and the WDK already gives
  context creation a private-data channel. `Adapter::escape` stays
  `not_supported`.
- **A submission is an RVKS descriptor in the DMA buffer.** The transport writes
  its batch into the ring, then submits a DMA buffer holding the 32-byte RVKS
  (begin, end, fence). The miniport reads it, stores the ring tail, and rings
  the doorbell. This reuses `RingSubmitter` and the descriptor the wire already
  defines, and it keeps submission ordered against everything else on the
  context's node.

## 3. Transport memory: where the ring, fence and reply live

041's parked placement means host-visible memory through VidMm is not available,
and the record shows options not yet tried. Rather than reopen that, this
feature takes the path that already works and that the device already accepts.

### Decision: aperture-segment allocations, published as guest blobs

A ring, fence page or reply allocated in the aperture segment with
`AccessedPhysically` is one VidMm hands the driver the pages for - the same way
a primary works today (`Allocation::map_aperture`) - so the miniport can create
a `RESOURCE_CREATE_BLOB` GUEST over those pages and name it in the setup. Three
allocations, three blob ids, which is what RVKC needs. `D3DKMTLock2` maps the
same allocation into the process, so the transport writes the ring the device
reads.

This is guest memory, not host-visible, so a reply is copied at the boundary
rather than mapped from the BAR. That is the honest state 041 left: the capset's
`feature_guest_memory_import` bit says the executor can import these pages
zero-copy (measured in 042), and `feature_host_visible_memory` stays what it is.
The blob-arena of host-visible memory the spec's US2 describes waits on 041's
placement problem being solved; US2 is reduced to what works, with the rest
recorded as unmet and why.

**Rejected:** kernel contiguous memory mapped out by an escape (a second entry
point and an MDL lifetime to reason about); a sub-range of the `0x1000` region
blob (RVKC has no offset); reopening VidMm host-visible placement (041's, and
not gated on the transport existing).

## 4. What the generated client demands, and the writer mismatch

The generated client dispatch does not compile, and not only for a missing
header: it default-constructs `::av::WireWriter`, calls `.bytes()`, builds a
`WireReader` from that, and default-constructs `::av::WireArena` - none of which
the fixed-capacity `wire_*` types have. It calls two transport members,
`send_sync(id, payload, &reply)` and `send_async(id, payload)`, against an
`av::TransportResult::Ok`; OpenGL additionally needs an `av::ClientBlobArena`
(a `Lease` with `.ok()`, `.data`, `.offset`; `for_transport`, `ready`, `take`,
`give_back`, `retire_after_last`).

### Decision: the transport lends the writer (option A), changed in the emitter

The emitter is ours (063). It changes so the client asks the transport for a
writer over the ring slot it is about to fill, and the transport hands back a
`WireWriter` over that block; `send_*` takes the writer's filled extent, and a
sync reply is a `WireReader` over the reply slot. `wire_*` is unchanged - no
`bytes()`, no default constructor, no heap - and the emitter stops assuming a
growable buffer.

This settles four things the client got wrong, all in one place:

- **`send_*` refuses a writer whose `status()` is not ok**, which is the
  overflow check Vulkan and OpenCL never emitted (OpenGL already checks). A
  frame that does not fit the slot is a fault, not a truncation.
- **The `WireArena` sites** take a `{nullptr, 0}` arena that refuses on
  purpose. On the guest a decoded reply points into the application's own
  structures; an arena allocation there would be scratch that dies with the
  call, which is exactly the pNext case, so refusing it is correct.
- **`ClientBlobArena::for_transport` becomes a transport member**, not a static
  (principle III), and the lease is how a large array reaches the host through a
  blob instead of inline - which is the one place US2's host-visible arena is
  actually used, and it degrades to inline when no arena is ready.
- **The emitter's own two bugs** - a `VkResult` failure value returned into an
  unsigned handle getter, and an OpenGL read-back lease not handed back on the
  failure path - are fixed with it.

**Rejected:** option B (043 provides a growable `ClientWriter`), which needs a
heap or a large per-call stack frame in thousands of functions, copies every
request twice, and still leaves the overflow check unwritten.

### What stays other features' to write, recorded not built

The generated client also names `av::icd::payload_dropped` (045),
`gl_helpers.h` and nine length helpers and `rvk_glGetError` (050), and
`mint_sync_token` (042's `gl_sync_table`, reached only through `gl_helpers.h`).
043 makes the Vulkan and OpenCL clients compile and link against the transport;
the OpenGL client needs 050's helpers and is left compiling against a stated
contract, not linking. `gl_sizes.h`'s one-argument `pname_count` versus the
two-argument call the client makes is 063's to reconcile when 050 arrives.

## 5. Waiting, and faults

A process waits today only on dxgkrnl paging fences and a CPU-notification
signalled from the driver's own DPC; no rvk fence reaches user mode, and the
fence-page interrupt path is not wired into the adapter's ISR.

### Decision: the driver waits on the fence page, and signals a monitored fence

The miniport gives each context's fence queue an MSI-X vector (the wire already
reserves queue `2+n` for it), reads the fence page in its DPC through the
existing `FenceObserver`, and completes the context's submission fence when the
page says the batch completed - rather than from its own DPC unconditionally.
A process then waits the way it already can, on that fence through
`D3DKMTWaitForSynchronizationObjectFromCpu`, and the wait is a real sleep. A
faulted page becomes `DXGK_INTERRUPT_DMA_FAULTED` for the owning context, which
is `ContextLoss`'s existing path, and recovery is `CTX_DESTROY` and a fresh
setup.

Monitored-fence and native-fence DDIs stay null: this uses the submission fence
dxgkrnl already tracks, signalled when the page says so, which needs no new DDI
slot. Doorbell DDIs also stay null - the doorbell store is the miniport's, on
submit, into the BAR the adapter now maps (FR-007's "doorbell-less submission"
from the guest's point of view: the guest submits, the driver rings).

## 6. The no-CRT build

The generated client includes `<cstdint>`/`<cstring>` and the API's own header,
none of which the no-CRT `rvktest` build has. 063 excluded generated sources
from that build for this reason. The transport itself is portable core with no
STL; it builds where `GuestContext` builds. The generated client is a separate
question that lands with the ICDs (045/050/052), which are ordinary user-mode
DLLs with a CRT, not the freestanding suite. 043 proves the transport over the
L4 loopback rig and a hand-written frame in the guest, not over the generated
client, whose build home is a later feature.

## 7. Scope this leaves 043

Deliver: private-data context creation wiring an rvk `GuestContext` into the
miniport; aperture-segment ring/fence/reply allocations published as guest
blobs and mapped to the process; RVKS submission read from the DMA buffer with
the doorbell rung by the driver; the fence-page interrupt and a real wait;
faults to `DMA_FAULTED`; the `ClientTransport` interface the generated client is
handed, with the emitter changed to lend the writer; and the Vulkan and OpenCL
clients compiling against it. Prove it over L4 and in a real guest.

Not deliver, recorded with the reason: a host-visible blob arena (041's parked
placement); the OpenGL client linking (050's helpers); the generated client in
the no-CRT suite (045+). SC-002's "100% branch coverage of the transport core"
holds; SC-003's "no busy polling" holds via the fence interrupt.
