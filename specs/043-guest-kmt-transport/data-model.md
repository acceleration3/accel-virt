# Data model: the guest transport

The wire and the device side are complete; the guest-side rvk state machine
(`GuestContext` and the four classes it owns) has existed since 034 and runs
only in the L4 loopback rig. Nothing joins a process to it. This feature is the
join, and a join is only safe if every lifetime across it is written down: who
maps the ring, how long the mapping stays a pointer, which thread reads the
fence page, and what a byte read from shared memory is checked against before it
is used. That is what this document fixes, before any of it is written.

Ten entities, the ten classes `plan.md` named. Four are the transport a process
holds; five are the miniport learning to serve an rvk context; one -
`RvkContextRecord` - is the record that crosses the private-data boundary and is
compiled by both sides. None of them is `GuestContext`, `RingSubmitter`,
`FenceObserver`, `ContextRequestSequence` or `ContextMemory`: those exist, and
this feature gives them their first production caller rather than rewriting them
(`plan.md`, "Classes this feature introduces").

Three words are used throughout with one meaning each:

- **user thread** - the guest process thread a generated client runs on. There
  is no IRQL there; the transport members it calls are ordinary user-mode code,
  and the contract's rule B5 (*"one transport, one context, one thread"*) is
  what keeps them serialized.
- **miniport** - `rvkgpu.sys` in kernel mode, whose IRQL ceilings are the ones
  `GuestContext` already carries: `set_up` and `release` at `PASSIVE`,
  `next_request`/`accept`/`submit`/`serve`/`tear_down` at `DISPATCH`, and the
  fence-page read reachable at the ISR's IRQL (`src/core/rvk/guest_context.hpp:39-59`,
  `src/core/rvk/fence_observer.hpp:5-7`).
- **shared memory** - the ring, reply slot and fence page, each written by the
  other side of the seam and validated on every read (Principle V; the device
  and the guest distrust each other's shared memory equally, `plan.md`
  Constraints).

## Where the entities live

| Entity | Kind | Side | Header (proposed) |
|---|---|---|---|
| `ClientTransport` | object (interface) | process | `src/proto/client_transport.hpp` |
| `KmtTransport` | object | process | `src/guest/transport/kmt_transport.hpp` |
| `TransportRing` | object | process | `src/guest/transport/transport_ring.hpp` |
| `ReplyReader` | object | process | `src/guest/transport/reply_reader.hpp` |
| `TransportFence` | object | process | `src/guest/transport/transport_fence.hpp` |
| `RvkContextRecord` | object | shared | `src/proto/context/rvk_context_record.hpp` |
| `RvkContextBinding` | object | miniport | `src/core/rvk/rvk_context_binding.hpp` |
| `ContextBlobPublisher` | system | miniport | `src/core/rvk/context_blob_publisher.hpp` |
| `SubmissionReader` | object | miniport | `src/core/adapter/submission_reader.hpp` |
| `FenceInterruptRoute` | system | miniport | `src/core/rvk/fence_interrupt_route.hpp` |

`src/proto/client_transport.hpp` is not a proposal: the committed generated
client already includes it and names `::av::ClientTransport`
(`src/proto/generated/vulkan/vk_dispatch_client.h:7-9`). The other headers are
this feature's to place, at the paths `plan.md`'s Project Structure names.

---

## `ClientTransport` (object, interface, process side)

The interface the generated client dispatch is handed, and the whole of what
`contracts/client_transport.md` fixes. It owns no state: it is a pure interface
with two real implementers - `KmtTransport` in a guest, the loopback rig's
transport in a test - which is why it is a class and not a template (`plan.md`
Complexity Tracking; the two implementations exist from the first test, the way
`Platform` has two). Its destructor is `protected` and non-virtual, because no
`ClientTransport` is ever destroyed through a base pointer and the kernel has no
`operator delete` (CLAUDE.md, Adaptations).

| Member (from the contract) | Meaning |
|---|---|
| `request_writer()` | a `WireWriter` over the room one command may fill (A1) |
| `send_async(id, writer)` | publish under `id`, no answer (A2) |
| `send_sync(id, writer, reply)` | publish and wait; `reply` is a `WireReader` the transport points at the answer (A3) |
| `decode_arena()` | the arena a reply decodes into; `{nullptr, 0}` on the guest (A4) |
| `blobs()` | the `ClientBlobArena` for a large array, OpenGL only (A5) |

Every call is on the user thread. The generated client holds a
`ClientTransport&` and never constructs one; a concrete transport is constructed
by whatever loads the client (its ICD, or the test rig). The `request_writer` /
`send_*(writer)` shape is the one the emitter change of research §4 produces; it
is documented here as the settled contract, not as what the committed generated
sources spell today (see the report at the end).

## `KmtTransport` (object, process side)

The transport a process holds over D3DKMT: the one real implementer of
`ClientTransport` in a guest. It carries one context's commands to the device
and answers back.

| State | Type | Meaning |
|---|---|---|
| `kmt_` | `Kmt &` | the D3DKMT seam, injected (`src/guest/kmt/kmt.hpp:19`) |
| `adapter_`, `device_`, `context_` | `uint32_t` | the `D3DKMT_HANDLE` values of the opened adapter, its device, and the virtual context |
| `ring_view_`, `reply_view_`, `fence_view_` | `void *` | the process's locked CPU views of the three allocations |
| `ring_` | `TransportRing` | by value; owns the ring block and the tail it has published |
| `reply_` | `ReplyReader` | by value; owns the reply block, or is unbound when the context has no reply slot |
| `fence_` | `TransportFence` | by value; the fence ids submitted and the last completed seen |
| `sub_api_` | `SubApi` | which of the three this context speaks (`src/proto/context/sub_api.hpp:9`) |
| `sync_object_` | `uint32_t` | the context's submission fence, waited on with a real sleep |
| `state_` | an enum | unbound / set up / running / faulted / torn down (below) |

Constructed when the first generated client of a sub-API in the process asks for
its link; a second client of the same sub-API shares it (spec US1.1). Fallible
work is two-phase (CLAUDE.md): a trivial constructor, then `initialize()`
returning `Status` - it opens the rvkgpu adapter, creates a device, creates the
three allocations (`AllocationKind::context_ring`, `fence_page`, and a `client`
reply, `src/proto/resources/allocation_kind.hpp:9-24`), locks each into the
process, encodes an `RvkContextRecord` and creates the virtual context carrying
it as private data. Destroyed at process teardown or when the last client of the
sub-API releases the link; destruction is the reverse of `initialize` (see
Ownership).

Every method runs on the user thread. The wait in `send_sync` is
`D3DKMTWaitForSynchronizationObjectFromCpu` on `sync_object_`, a real sleep the
miniport signals when the device's fence page says the batch finished, so no
thread spins (contract C, SC-003).

## `TransportRing` (object, process side)

Puts one batch in the ring. It is the user-mode counterpart to the miniport's
`RingSubmitter` (`src/core/rvk/ring_submitter.hpp`): `TransportRing` writes the
bytes into the mapped ring and tracks the tail it has produced, while the
miniport's `RingSubmitter` reads that tail from the DMA-buffer submission and
publishes it to the device.

| State | Type | Meaning |
|---|---|---|
| `ring_` | `uint8_t *` | the mapped ring block: 32-byte header (`RVKR`), then `2^order` bytes of data |
| `order_` | `uint8_t` | the ring order, 12..24 (`src/proto/submission/ring_header.hpp:13-14`) |
| `tail_` | `uint64_t` | total bytes produced, the value stored to the header's `tail` at offset 8 |
| `batch_begin_` | `uint64_t` | the tail counter at the open batch's `RVKB` header, so the batch's length is closed against it |
| `open_` | `bool` | whether a batch header is open and awaiting more commands |

Constructed with the `KmtTransport` once the ring is mapped and its fresh header
written (magic, order, head 0, tail 0; the driver sequence step 2,
`docs/protocol.md`). Destroyed with the transport. `request_writer` hands out
the next free span of the ring's data area as a `WireWriter`; `send_async`
advances the tail and frames a `BatchHeader` (`RVKB`, 16 bytes, offset 0 magic,
4 length, 8 fence_id, `src/proto/submission/batch_header.hpp`) when the batch
closes; `send_sync` closes the batch, hands the `SubmissionDescriptor` to
`KmtTransport` to submit. Nothing here blocks or allocates.

## `ReplyReader` (object, process side)

Reads one answer, refusing a slot no device wrote.

| State | Type | Meaning |
|---|---|---|
| `reply_` | `uint8_t *` | the mapped reply block, or null when the context named no reply slot |
| `capacity_` | `uint32_t` | `reply blob size - 32`, the answer bytes the slot can carry |
| `last_fence_` | `uint64_t` | the `fence_id` of the answer last read, to match a reply to its batch |

Constructed with the `KmtTransport`, unbound (null block) when the context has
no reply slot. Destroyed with the transport. It reads the 32-byte `RVKY` header
(offset 0 magic, 4 status, 8 fence_id, 16 size, 20 required, 24 reserved;
`src/proto/submission/reply_header.hpp`, `docs/protocol.md`), and on `status_ok`
hands the client a `WireReader` over the `size` answer bytes that follow at
offset 32. A `status_too_large` header carries `required` and no bytes, and
`send_sync` reports that length rather than a reader over nothing (contract B3,
edge case "a reply larger than the reply slot holds"). The answer is valid only
until the next `send_sync`, which overwrites the slot (contract B2).

## `TransportFence` (object, process side)

Says which submission has completed, from the fence page.

| State | Type | Meaning |
|---|---|---|
| `fence_page_` | `const uint8_t *` | the mapped fence page (`RVKF`), read-only |
| `submitted_` | `uint64_t` | the highest fence id this transport has published |
| `completed_` | `uint64_t` | the highest completed fence it has observed |
| `outstanding_` | a fixed array of `(fence_id, ring end)` | the batches submitted but not yet completed, so a full-ring send can wait on the oldest |
| `count_` | `uint32_t` | how many are outstanding |
| `faulted_` | `bool` | whether the page has read `faulted` |

Constructed with the `KmtTransport`, destroyed with it, read on the user thread.
It reports the last published and the last completed sequence number (FR-001,
US1.3); the completed number comes from the fence page's `completed_fence` at
offset 8, judged against `submitted_` the same way `FenceProgress` judges it in
the miniport (`src/proto/submission/fence_progress.hpp:17-30`). The blocking
wait itself is `KmtTransport`'s, on the submission fence; `TransportFence` is
the record that says whether a wait is even needed and, when the ring is full,
which fence to wait for.

## `RvkContextRecord` (object, shared side)

The private data of one `D3DKMTCreateContextVirtual`. The transport encodes it
in user mode; the miniport decodes it at `PASSIVE` inside `DxgkDdiCreateContext`
and treats every byte as hostile, exactly as `AllocationDescriptor` (RVKA,
`src/proto/resources/allocation_descriptor.cpp:60-89`) and `ContextSetup` (RVKC,
`src/proto/context/context_setup.cpp:62-77`) already do. It says what host
context a `CreateContext` asked for.

### Proposed byte layout - 32 bytes

Modelled field for field on RVKC (`docs/protocol.md`, Context setup), with one
deliberate difference: it carries **no doorbell**, because the miniport owns the
doorbell page and rings it itself (research §5), and it names the context's
**D3DKMT allocations** rather than virtio-gpu resource ids, because the
virtio-gpu resource ids do not exist until the miniport publishes the blobs
(`ContextBlobPublisher`, below).

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `RVKT` = `0x544B5652` (proposed here, settled in tasks) |
| 4 | u16 | major | the version the transport built against; compatible with the miniport |
| 6 | u16 | minor | |
| 8 | u32 | sub_api | exactly one of `SubApi` {1 Vulkan, 2 OpenGL, 4 OpenCL}, one bit only (`src/proto/context/sub_api.hpp:9`) |
| 12 | u32 | ring | the ring allocation, non-zero |
| 16 | u8 | ring_order | within 12..24 |
| 17 | u8[3] | reserved | 0 |
| 20 | u32 | fence | the fence-page allocation, non-zero, different from `ring` |
| 24 | u32 | reply | 0 for no reply slot; otherwise different from `ring` and `fence` |
| 28 | u32 | reserved | 0 |

Whether `ring`/`fence`/`reply` are D3DKMT allocation handles or indices into the
context's allocation list is settled in tasks; the layout and the rules below do
not depend on which.

### Validation, on the miniport side, of hostile bytes

Decoded the way RVKA and RVKC decode: read every field with `WireReader`,
`expect_zero` the two reserved runs, then refuse unless magic and every field
check pass; the record is unchanged unless the decode returns `ok`
(`allocation_descriptor.cpp:76-88`). The field checks mirror
`ContextSetup::fields_valid` (`context_setup.cpp:53-63`) exactly:

- a bad magic is `malformed`;
- an unknown `sub_api` (not exactly one of {1, 2, 4}) is `malformed`;
- `ring_order` outside 12..24 is `malformed`;
- `ring` or `fence` zero, or `fence == ring`, is `malformed`;
- a non-zero `reply` that equals `ring` or `fence` is `malformed`.

The checks that need the device's capset - major-version compatibility,
`sub_api` membership in the capset's `sub_apis`, `ring_order` inside the
capset's `[min, max]` - are a second pass against the decoded capset, exactly as
`ContextSetup::validate_against` splits them out (`context_setup.cpp:79-85`); a
failure there is `not_supported`, and the context is refused.

## `RvkContextBinding` (object, miniport side)

Binds one dxgkrnl context to an rvk context: it owns the `GuestContext` a
dxgkrnl context runs on, and the three blobs behind it.

| State | Type | Meaning |
|---|---|---|
| `context_` | `GuestContext` | by value; the rvk state machine (`src/core/rvk/guest_context.hpp:31`) |
| `record_` | `RvkContextRecord` | the decoded private data this binding was built from |
| `blobs_` | three virtio-gpu resource ids | the ring, fence and reply blobs `ContextBlobPublisher` pinned |
| `sync_object_` | the scheduling context's submission fence | signalled when the fence page says a batch completed |

Constructed at `DxgkDdiCreateContext`, at `PASSIVE`, when an `RvkContextRecord`
decodes and validates against the capset; `set_up` drives `GuestContext::set_up`
(`guest_context.hpp:40`) with a `ContextSetup` built from `record_` and the ids
`ContextBlobPublisher` assigned. `submit` reads the DMA buffer through
`SubmissionReader` and calls `GuestContext::submit` at `DISPATCH`; `serve`
observes the fence page at `DISPATCH` from the DPC and completes `sync_object_`
when `completed_fence` advances. A faulted page raises
`DXGK_INTERRUPT_DMA_FAULTED` for the context, which is `ContextLoss`'s existing
path (research §5). Destroyed at `DxgkDdiDestroyContext`, at `PASSIVE`, after
`GuestContext::tear_down` and `release`. Its state is `GuestContext::State`
(`guest_context.hpp:33`), and the state machine below is that enum.

## `ContextBlobPublisher` (system, miniport side)

Turns a context's ring/fence/reply allocations into blobs the device names. One
per adapter, injected where a binding is created.

| State | Type | Meaning |
|---|---|---|
| `pins_` | a fixed table of pinned aperture allocations, by context | the pages VidMm handed the driver, held while the binding lives |
| `next_id_` | `uint32_t` | the next virtio-gpu blob resource id to assign (above the 1..15 reserved range, `docs/protocol.md`) |

Constructed with the adapter, destroyed with it. For each of a context's three
allocations it pins the aperture-segment pages the same way a primary is pinned
(`Allocation::map_aperture`, research §3), creates a `RESOURCE_CREATE_BLOB`
GUEST over those pages, and returns the three distinct non-zero resource ids the
`ContextSetup` needs (RVKC requires three distinct ids, `context_setup.cpp:58-62`).
The pins are released when the binding tears down, not before: a blob the device
holds may not be unreferenced until `CTX_DESTROY` (`docs/protocol.md`, "Capset
and context commands"). Runs at `PASSIVE` (blob creation is context setup).

## `SubmissionReader` (object, miniport side)

Turns a submitted DMA buffer into an `RVKS` the ring submitter can act on.

| State | Type | Meaning |
|---|---|---|
| `platform_` | `Platform &` | injected; how the DMA buffer's guest bytes are read |

`plan.md` lists its state as "none; reads a DMA buffer"; it owns no resource,
but it holds the `Platform` it reads through, because a class with no members is
forbidden (CLAUDE.md) and the bytes must be copied once before they are
validated (`docs/protocol.md`, Conventions). Runs at `DISPATCH`, from
`Adapter::submit_command_virtual` (`src/core/adapter/adapter.cpp:1149`), which
today admits the DMA buffer's VA but never reads it (`adapter.cpp:1154`). It
decodes the 32-byte `SubmissionDescriptor` (`RVKS`, offset 0 magic, 4 flags, 8
fence_id, 16 begin, 24 end; `src/proto/submission/submission_descriptor.hpp`)
and hands the `GuestContext` a validated descriptor; a buffer that does not
decode faults the context rather than being trusted.

## `FenceInterruptRoute` (system, miniport side)

Wakes the context whose fence page the device just wrote. One per adapter.

| State | Type | Meaning |
|---|---|---|
| `bindings_` | `RvkContextBinding *[fence_queues]` | which context each fence-queue vector serves, or null |

Constructed with the adapter, destroyed with it. When the device interrupts on a
context's fence queue (`docs/protocol.md`, "Fence queues and interrupts"), the
ISR path looks the vector up in `bindings_` and schedules that context's
`serve`. An entry is registered when a binding is set up and cleared when it
tears down. The read runs at the ISR's IRQL and the `serve` it schedules at
`DISPATCH`; neither allocates or blocks (Principle VII).

---

## Ownership and lifetime

```
Process (user mode)
  KmtTransport                one per sub-API context the process uses
  ├── TransportRing           the mapped ring block and its tail
  ├── ReplyReader             the mapped reply block, or unbound
  └── TransportFence          submitted/completed fence ids, outstanding table
        (all three are views into locked allocations; valid only while locked)

Miniport (kernel)
  Adapter
  ├── ContextBlobPublisher    the aperture pins, one adapter-wide service
  ├── FenceInterruptRoute     vector -> binding, one adapter-wide service
  └── RvkContextBinding[]      one per dxgkrnl virtual context
        └── GuestContext       (034) owns ContextMemory, the sequence,
                               RingSubmitter, FenceObserver, FenceProgress
```

Order, outermost first: the process outlives every transport; a `KmtTransport`
outlives its ring, reply and fence; the adapter outlives every binding; a
binding outlives its `GuestContext`. The record `RvkContextRecord` is not owned
by either side for long - the transport encodes it into the create call and the
miniport copies it into the binding, the way `import_blob` copies a handle
record in feature 042.

### What a mapped view must not outlive

- **A ring, reply or fence view is a pointer only while its allocation is
  locked and the context lives.** `KmtTransport` holds each view from the
  `Kmt::lock` in `initialize` (`src/guest/kmt/kmt.hpp:34-36`) until the matching
  `unlock` in teardown; a read after unlock, or after the miniport tore the
  context down, is a read of memory that is no longer there (edge case "the
  adapter is removed while a process holds a link"). `TransportRing`,
  `ReplyReader` and `TransportFence` never keep a copy of the pointer past the
  transport that owns the lock.
- **A `ReplyReader`'s answer is a `WireReader` valid only until the next
  `send_sync`** (contract B2): it points into the reply slot, which the next
  synchronous call overwrites. The client reads before it sends again.

### Teardown order - the reverse of setup

Setup, process side: open adapter -> create device -> create the ring, fence and
reply allocations -> lock each into the process -> create the virtual context
with the `RvkContextRecord`. The miniport's half then runs the driver sequence
of `docs/protocol.md` step 3: `CTX_CREATE`, a blob for each allocation, then the
`ContextSetup`.

Teardown reverses it, matching `GuestSession::tear_down`
(`src/guest/suite/guest_session.hpp:44`, "released in reverse ... whatever
failed") and `ContextRequestSequence::begin_teardown`
(`src/core/rvk/context_request_sequence.hpp`): drain outstanding submissions ->
destroy the virtual context, which drives `GuestContext::tear_down` (`CTX_DESTROY`,
then `RESOURCE_UNREF` for each blob, in order,
`context_request_sequence.hpp:1-8`) -> unlock the three views -> destroy the
three allocations -> destroy the device -> close the adapter. Every step is
attempted even after an earlier one fails, and a handle answered as 0 is treated
as failure and not kept (`guest_session.hpp:55-66`).

## Validation rules

Quoted from `docs/protocol.md` and `contracts/client_transport.md` where they
state them, so that a change to either shows up here as a quotation that no
longer matches. Each is a rule on the entity that enforces it.

| # | Rule (verbatim) | Source | Who enforces it |
|---|---|---|---|
| V1 | *"At every observation `head <= tail <= head + 2^order`, and neither counter is smaller than the last value observed."* | protocol.md, Ring header | the miniport's `RingSubmitter::judge_head` (`ring_submitter.hpp:57`); `TransportRing` never advances `tail_` past `head + 2^order`, and waits when it would (edge case "the ring is full") |
| V2 | *"fence_id ... >= 1, greater than the previous batch's on this context."* | protocol.md, Batches | `BatchHeader` when `TransportRing` frames a batch; `TransportFence.submitted_` supplies the next id; `FenceProgress::record_submission` (*"Fence ids are strictly increasing from 1"*, `fence_progress.hpp:16`) is the miniport's check |
| V3 | *"a sequence of commands, each framed by the header below, filling the payload exactly."* | protocol.md, Execution | `TransportRing`: a batch's `length` is the sum of the command sizes written, so the payload ends exactly on the last command; the device faults the context otherwise (fault code 5) |
| V4 | *"A reply blob, when named, must hold at least 64 bytes"* and *"The slot holds `reply blob size - 32` bytes of answer, at most 16 MiB."* | protocol.md, Capset/context and Reply slot | `ReplyReader.capacity_`; `send_sync` reports too-large rather than truncating (contract B3, D3: *"the reply slot's 16 MiB ceiling and 64-byte floor are the wire's"*) |
| V5 | RVKC's *"ring_resource ... non-zero"*, *"fence_resource ... non-zero, different from ring_resource"*, *"reply_resource ... 0 ... otherwise different from ring_resource and fence_resource."* | protocol.md, Context setup | `RvkContextRecord` decode, mirroring `ContextSetup::fields_valid` (`context_setup.cpp:57-62`); the three ids are distinct and non-zero before a context is bound |
| V6 | *"A writer that overflowed is never sent. `send_*` checks `status()`; not `ok` is a dropped frame and a faulted context."* | contract B1 | `KmtTransport::send_async`/`send_sync`, checking `WireWriter::status()` (`src/proto/wire/wire_writer.hpp:52`) |
| V7 | *"`completed_fence` and `progress` never decrease, `completed_fence` never exceeds the highest fence id submitted, and once `faulted` is observed it stays set."* | protocol.md, Fence page | `FenceProgress::observe` in the miniport (`fence_progress.hpp:19`); `TransportFence` judges the mapped page against `submitted_` before it reports a completion, and faults the transport on a violation (edge case "the fence page holds hostile values") |

Everything in that table is attacker-chosen: the ring, reply and fence page are
the other side's to write, and each rule is checked on every read.

## State transitions

### `KmtTransport` (process side)

```
          initialize ok        first send        fault
unbound ──────────────▶ set up ─────────▶ running ─────────▶ faulted
   │                       │                  │                  │
   │ initialize fails      │                  │                  │
   └───────────────────────┴──────────────────┴──────── release ┴──▶ torn down
```

| From | To | What causes it |
|---|---|---|
| unbound | set up | `initialize`: the adapter opened, the device and allocations were created and locked, and the miniport accepted the `RvkContextRecord` and established the `GuestContext` |
| unbound | torn down | `initialize` failed at any step; every partial acquisition is released in reverse and the transport is dead |
| set up | running | the first `send_async`/`send_sync`, or immediately on a transport that only reports counters |
| running | running | a `send_*` that published, a wait that completed, a full-ring `send` that waited and then published |
| running | faulted | the mapped fence page reads `faulted` (RVKF flag bit 0); or `completed_fence` moves backward or exceeds `submitted_`, `progress` moves backward, or a reply's `RVKY` magic/status is wrong (V7, V4); or any D3DKMT call returns device-lost (adapter removed, TDR); or a full-ring wait exceeds its timeout |
| faulted | faulted | any further `send_*` returns not-`Ok` without touching shared memory; `connected()` reads false; the client reports device lost (spec US3.1) |
| running or faulted | torn down | teardown: the context is destroyed, the views unlocked, the allocations and device destroyed, the adapter closed |

### `RvkContextBinding` (miniport side, the same seam)

This is `GuestContext::State` (`guest_context.hpp:33`), which the binding wraps
one to one:

```
        set_up ok          setup refused / fault        tear_down    release
idle ──▶ setting_up ──▶ established ──────────────▶ failed ──▶ tearing_down ──▶ torn_down
              │                                        ▲
              └──────── setup refused ─────────────────┘
```

| From | To | What causes it |
|---|---|---|
| idle | setting_up | `DxgkDdiCreateContext` decoded a valid `RvkContextRecord`, `ContextBlobPublisher` pinned the three blobs, and `GuestContext::set_up` began the sequence |
| setting_up | established | the setup sequence's last answer established the context |
| setting_up | failed | a refused setup step (`ContextRequestSequence::refused`) |
| established | established | `submit` (via `SubmissionReader`), or `serve` observing progress |
| established | failed | `serve` observes a faulted fence page: `FenceObserver` fails the context (`fence_observer.hpp:5-7`), and the binding raises `DXGK_INTERRUPT_DMA_FAULTED` |
| established or failed | tearing_down | `GuestContext::tear_down` begins releasing what the device holds |
| tearing_down | torn_down | `release` frees the memory once the device let go of it; `abandon` on a device reset jumps straight here |

## Fixed bounds

There is no heap on the submission path, so every collection has a compile-time
maximum. Two of the four below already exist in the tree.

| Constant | Value | Where | Reasoning |
|---|---|---|---|
| `TransportRing` / `RvkContextRecord` ring order | **12..24** | **found, not proposed** | `RingHeader::lowest_order`/`highest_order` (`ring_header.hpp:13-14`), `ContextSetup` (`context_setup.hpp`), and the capset's `min_ring_order`/`max_ring_order` all agree; the ring data area is 4 KiB..16 MiB |
| `FenceInterruptRoute` fence-queue vectors | **64** | **found, not proposed** | `RingSubmitter::doorbell_pages` is 64 (`ring_submitter.hpp:26`); the device has 66 virtqueues - control, cursor, and one fence queue per doorbell page, `queue 2 + n` for doorbell `n` (`docs/protocol.md`, Fence queues) - so there are exactly 64 fence queues, one per possible context |
| `TransportFence` max outstanding submissions | **64** | proposed here, settled in tasks | A full-ring `send` waits on the fence of the oldest unconsumed record (edge case), so the transport must map a ring byte-offset back to a fence id; 64 in-flight batches is past any real client's submission depth, and a `(fence_id, end)` pair is 16 bytes, so the table is 1 KiB |
| `ReplyReader` reply capacity floor / ceiling | **64 B / 16 MiB** | **found, not proposed** | the wire's own: a reply blob *"must hold at least 64 bytes"* and the slot holds *"at most 16 MiB"* of answer (`docs/protocol.md`, Capset/context and Reply slot; `RingHeader::highest_order` is 24) |

One note for whoever settles the outstanding-submissions bound: the ring's byte
capacity (`2^order`, up to 16 MiB) already bounds how many bytes can be in
flight, but not how many *batches*, which is what a full-ring wait must index.
The number is a policy on submission depth, not a wire constant, which is why it
is proposed rather than derived.

## What is deliberately absent

| Absent | Why |
|---|---|
| A host-visible blob arena (`KmtBlobArena` / `ClientBlobArena`) | 041 parked host-visible placement: VidMm refuses every allocation whose preferred segment is CPU-visible (research §1, `host-visible-041-state`). The transport uses aperture-segment allocations published as guest blobs instead, and the arena the OpenGL write path needs is a follow-task added when its caller arrives, not built before one (`plan.md`; Principle VIII) |
| A BAR-mapped reply slot | the ring, reply and fence are guest memory, not host-visible, so a reply is copied at the boundary rather than mapped from the BAR (research §3). `ReplyReader` reads a locked guest allocation, and the capset's `host_visible_memory` bit stays what 041 left it |
| The OpenGL client linking | it needs 050's `gl_helpers.h`, its nine length helpers, `rvk_glGetError` and `mint_sync_token`, none of which exist (research §4, §7). 043 makes the Vulkan and OpenCL clients compile and link against the transport; the OpenGL client compiles against a stated contract and links when 050 arrives |
| A native-fence or monitored-fence DDI | the miniport reuses the scheduling context's submission fence, signalled when the fence page says the batch completed; a new DDI slot with one implementation is the speculative structure the constitution forbids (`plan.md` Constitution Check; research §5) |
| A doorbell in `RvkContextRecord` | RVKC carries a `doorbell` field; the record omits it because the miniport owns the doorbell page and rings it on submit (FR-007, research §5), so the process never names one |
