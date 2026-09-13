# Contract: `ClientTransport`

The generated client dispatch for all three APIs is handed a `ClientTransport&`
and calls a small set of things on it. This is what each means, and what the two
implementations - `KmtTransport` in a guest, the loopback rig's in a test - must
both keep. The generated code is `av::vkproto` / `av::clproto` / `av::glproto`;
the transport is `av::ClientTransport`.

## A. What the generated client calls

| # | Call | Meaning |
|---|---|---|
| A1 | `writer = t.request_writer()` | A `WireWriter` over the room one command may fill. The client encodes into it; a field that does not fit latches `too_large`, and the client does not check per field. |
| A2 | `t.send_async(id, writer)` | Publish the encoded command under `id`; no answer, result discarded. A writer that overflowed is a dropped frame that faults the context, never a truncated command. |
| A3 | `t.send_sync(id, writer, reply)` | Publish and wait. `reply` is a `WireReader` the transport points at the answer. `TransportResult::Ok`, or not-`Ok` for an overflow, a faulted context, or an answer that did not fit. |
| A4 | `arena = t.decode_arena()` | The arena a reply decodes into. On the guest it refuses every allocation: a decoded reply fills the application's own structures, and anything a decode allocates is scratch that dies with the call. `{nullptr, 0}` is a correct implementation. |
| A5 | `t.blobs()` (OpenGL only) | The `ClientBlobArena` a large array is lent room in, to cross as a blob not inline. `ready()` false means fall back to inline. |

`send_async` covers 220 Vulkan, 7 OpenCL, 1119 OpenGL commands; `send_sync`
covers 166, 98, 255.

## B. What both implementations guarantee

- **B1.** A writer that overflowed is never sent. `send_*` checks `status()`;
  not `ok` is a dropped frame and a faulted context. This is the check Vulkan
  and OpenCL never emitted and OpenGL always did, now in one place.
- **B2.** A reply reader is valid until the next call on the same transport; it
  points into the reply slot, which the next `send_sync` overwrites. The client
  reads before it sends again.
- **B3.** `send_sync` returns not-`Ok` rather than a reader over nothing: a
  too-large answer or a faulted context is a failed call.
- **B4.** `request_writer` hands out room for one command at a time; a second
  before the first is sent is a misuse the client never commits.
- **B5.** One transport, one context, one thread, matching the device's "one
  context on one worker".

## C. What `KmtTransport` adds over D3DKMT

- `request_writer` hands out the next free span of the mapped ring; `send_async`
  advances the tail and frames a batch when the batch closes; `send_sync`
  closes the batch, submits a DMA buffer holding the `SubmissionDescriptor`, and
  waits on the context's submission fence.
- The wait is `D3DKMTWaitForSynchronizationObjectFromCpu`, a real sleep: the
  driver completes that fence when the device's fence page says the batch
  finished, so no thread spins (SC-003).
- A faulted fence page becomes a failed `send_sync` and a context that answers
  not-`Ok` afterwards - `ContextLoss` seen from user mode.

## D. What does not change

- `wire_*` is untouched: fixed capacity, no `bytes()`, no default constructor,
  no heap. The transport owns the blocks; writer and reader are views.
- The command id's top 16 bits stay the API; the transport sends no command of
  another API than its context's.
- The reply slot's 16 MiB ceiling and 64-byte floor are the wire's; `send_sync`
  reports too-large rather than truncating.
