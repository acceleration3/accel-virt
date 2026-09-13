# Contract: `rvk_executor.h` version 2

The device owns this contract. A library that satisfies it is interchangeable
with any other, which is the whole point of the conformance suite: the device
must not care whether it loaded `rvk_echo_executor` or this project's.

Version 1 stays exactly as it is. A version 1 library keeps working, with no
blob import, and the device accepts both (FR-006). Everything below is what
version 2 adds and what both versions must already do.

## A. What version 1 already promised, restated as testable statements

| # | Statement | How the suite checks it |
|---|---|---|
| A1 | `rvk_executor_ops()` answers a non-null table whose `abi_version` the device knows | Load, call, compare; a null answer must fail realize, not yield a capset-less device |
| A2 | Every function pointer in the table is non-null | The device already refuses otherwise; the suite asserts the refusal message |
| A3 | `sub_apis` is answerable with no GPU context, on the main loop, before any `create` | Call it first, before anything else, and require an answer |
| A4 | `sub_apis` answers only bits the library will accept in `create` | For each advertised bit, `create` with a real GPU UUID must not fail for "unsupported" |
| A5 | `create` answers null rather than a broken executor | Ask for a UUID no GPU has; require null, not a handle that faults later |
| A6 | `execute` tolerates `reply == NULL, reply_capacity == 0` for a command with no reply flag | Send one; require `RVK_EXECUTED` and no write |
| A7 | An answer longer than `reply_capacity` writes nothing and sets `*reply_size` to the length needed | At the ABI: call with a capacity one byte short of the answer, require `RVK_EXECUTED`, `*reply_size` equal to the full length, and the reply block untouched (a poisoned block still poisoned). The device's `status_too_large` reply header is a second statement one layer up, checked by the qtest that already exists (`tests/qemu/test_virtio_gpu_execution.py:104-111`) - `execute` has no too-large result of its own in either version |
| A8 | `execute` does not retain `payload` past the call | The suite overwrites the payload block immediately after and runs another command |
| A9 | Every call returns | Each call is made under a watchdog; a call that has not returned inside the bound fails the suite rather than hanging it |
| A10 | A result outside the defined set is not invented | Any value above the highest defined result fails the suite |

A9 and A10 are the two the current device cannot enforce for itself; the suite
is where they become checkable at all.

## B. The memory channel, which is why there is a version 2

An executor sees one command's payload and nothing else. A command that names a
blob therefore has nothing to name it with, so the import happens out of band,
before `execute`, keyed by the id the guest used.

### B1. The handle

```c
#define RVK_BLOB_HOST_POINTER 1u   /* `pointer`, page-aligned, stays mapped */
#define RVK_BLOB_DMA_BUF      2u   /* `fd`, owned by the callee on success  */

typedef struct RvkBlobHandle {
    uint32_t kind;        /* exactly one RVK_BLOB_* value                  */
    uint32_t reserved;    /* 0                                             */
    void    *pointer;     /* RVK_BLOB_HOST_POINTER; NULL otherwise         */
    int      fd;          /* RVK_BLOB_DMA_BUF; -1 otherwise                */
    uint64_t size;        /* bytes, a whole number of host pages           */
} RvkBlobHandle;
```

**There is no offset.** The device hands over a handle that names exactly the
range the guest asked for: for a dma-buf that is `udmabuf_create.offset` at
creation, and for a pointer it is `base + offset`, page-aligned. OpenCL's import
has no offset property at all, so an offset in the ABI would have bought a
capability flag and a branch per API for nothing (research §3).

### B2. The three operations

```c
uint32_t (*import_kinds)(void *opaque, uint32_t sub_api, const uint8_t *gpu_uuid);
uint32_t (*import_blob)(void *opaque, RvkExecutor *executor,
                        uint64_t blob_id, const RvkBlobHandle *handle);
void     (*release_blob)(void *opaque, RvkExecutor *executor, uint64_t blob_id);
```

- `import_kinds` answers a mask of `RVK_BLOB_*` bits, or zero for "this
  sub-API on this GPU imports nothing; copy at the boundary". **It is on the
  table, not on an executor, and it must be answerable on the main loop during
  realize**, because the capset is described before any context exists.
- `import_blob` answers `RVK_EXECUTED` or `RVK_UNDECODABLE`. On success the
  callee owns an `RVK_BLOB_DMA_BUF` handle's `fd` and must close it; on failure
  the caller still owns it. A host pointer is never owned: the device keeps it
  mapped until `release_blob` or `destroy` returns.
- `release_blob` is idempotent for an id that was never imported. `destroy`
  releases everything still held, so a guest that never unrefs leaks nothing.

### B3. What the device guarantees before each call

| Before | The device has already |
|---|---|
| `import_blob` | built the handle on the main loop and cached it; pinned the blob so it cannot be unmapped while imported; checked `size` is a whole number of pages |
| `release_blob` | stopped naming that id in any command it will send afterwards |
| `destroy` | stopped the ring and will make no further call on that executor |

### B4. Threading

Version 1's rule stands: `sub_apis` on QEMU's main loop, `create`, `execute`
and `destroy` on that context's worker, one call at a time.

`import_kinds` joins `sub_apis` on the main loop. **`import_blob` and
`release_blob` join `execute` on the worker**, over a handle the device built
on the main loop earlier. The alternative - declaring them main-loop-only -
would need a serialisation against a worker sitting inside `execute`, which is
a second rule for no gain.

## C. What a fault means

| Result | Meaning | The device's response, today |
|---|---|---|
| `RVK_EXECUTED` | ran; `*reply_size` bytes of answer, or none | heartbeat, continue |
| `RVK_UNKNOWN` | an id this library does not decode | skip, continue |
| `RVK_UNDECODABLE` | the payload does not decode, **or the host would not give it room** | fault code 6, context stops |
| `RVK_DEVICE_LOST` | this executor's host device is gone | fault code 7, context stops |

Two statements the implementation must keep, and the suite must check:

- **Arena exhaustion answers `RVK_UNDECODABLE`.** A frame that needs more room
  than this host will give is a frame this host cannot take. The two cases are
  not distinguishable on the wire on purpose - telling a guest how big the
  host's arena is tells an attacker where the edge is - but they are
  distinguishable in the log and in `DecodeArena`'s refusal count, which is what
  the suite asserts (research §2).
- **`RVK_DEVICE_LOST` is per executor, not per device.** The device has no
  device-wide loss path and a qtest asserts other contexts survive. A library
  whose host device is shared across contexts must therefore answer it for each
  affected executor as that executor is next used, not once.

## D. What does not change

- The command id's top 16 bits remain the API, and the device filters to one
  API per context before calling, so an executor never sees another API's id.
- One reply per batch, the last reply-flagged command's answer. The ABI gains no
  way to volunteer an answer, no fence parameter and no asynchronous completion.
- `RVK_EXECUTOR_OPS_SYMBOL` stays `"rvk_executor_ops"`, and the table is still
  fetched by calling the function that symbol names.
