# Data model: the host executor

Version 1's data was one borrowed payload and nothing else. Version 2 adds a
memory channel, and a channel is only safe if every lifetime in it is written
down: who builds the handle, who owns the fd afterwards, how long a pointer
stays a pointer, and which thread is allowed to ask. That is what this document
fixes, before any of it is written.

Fourteen entities, the fourteen classes `plan.md` named. Nine are the library,
two are the device side, one is the suite, and two - `HostGpu` and `HostGpuSet` -
are what makes the capability query answerable at realize.

Two words are used throughout with exactly the contract's meaning
(`contracts/executor_abi.md` §B4, which is normative):

- **main loop** - QEMU's main loop, with the big lock held. `sub_apis` and
  `import_kinds` only.
- **worker** - the single thread of the one context this session belongs to.
  `create`, `execute`, `destroy`, `import_blob` and `release_blob` only.

The device checks the distinction as far as it can: `TableHost::require_worker`
tests only that the big lock is *not* held
(`src/device/abi/table_host.cpp:25-28`), so it cannot tell one worker from
another. Research §1 decided to leave that gap and record it; the entities below
are written so that telling workers apart is never necessary - no state is
shared between two sessions.

## Where the entities live

| Entity | Kind | Side | Header |
|---|---|---|---|
| `HostGpu` | object | library | `src/device/executor/host_gpu.hpp` |
| `HostGpuSet` | system | library | `src/device/executor/host_gpu_set.hpp` |
| `ExecutorLibrary` | system | library | `src/device/executor/executor_library.hpp` |
| `ContextSession` | object | library | `src/device/executor/context_session.hpp` |
| `DecodeArena` | object | library | `src/device/executor/decode_arena.hpp` |
| `VulkanBackend` | object | library | `src/device/executor/vulkan_backend.hpp` |
| `OpenGlBackend` | object | library | `src/device/executor/opengl_backend.hpp` |
| `OpenClBackend` | object | library | `src/device/executor/opencl_backend.hpp` |
| `ImportedBlob` | object | library | `src/device/executor/imported_blob.hpp` |
| `ImportTable` | system | library | `src/device/executor/blob_table.hpp` |
| `GuestRegion` | object | library | `src/device/executor/guest_region.hpp` |
| `SyncTokens` | system | library | `src/device/executor/sync_tokens.hpp` |
| `BlobHandleSource` | system | device | `src/device/memory/blob_handle_source.hpp` |
| `ExecutorConformance` | system | test | `tests/conformance/executor/executor_conformance.hpp` |

**Two of these names already exist in `namespace av`.** Both collisions are
real and both are reported, not resolved, here:

- `ExecutorLibrary`. The existing one is the **device's**
  (`src/device/execution/executor_library.hpp:15`): it holds the
  `const RvkExecutorOps *` the glue handed over, checks it
  (`src/device/execution/executor_library.cpp:7-16`) and calls through it. The
  one this feature adds is the **library's**: it is what sits behind `opaque`
  and answers that table. They are opposite ends of the same ABI and never
  compile into the same translation unit, but they share a namespace and a
  name, and a reader who greps will find the wrong one.
- `ImportTable`. The existing one is the device's blob resources by guest
  resource id (`src/device/memory/blob_table.hpp:22`, capped at 1024 -
  `blob_table.hpp:24`). The one this feature adds holds one session's
  `ImportedBlob`s by blob id. Same namespace, same name, different side of the
  ABI.

---

## `HostGpu` (object)

One host GPU, as the library found it.

| State | Type | Meaning |
|---|---|---|
| `uuid_` | `uint8_t[16]` | the Vulkan device UUID, the only name the ABI carries (`create` and `import_kinds` both take `const uint8_t *gpu_uuid`; the device's own copy is `ContextExecutor::uuid_[16]`, `src/device/execution/context_executor.hpp:22,59) |
| `render_node_` | `char[32]` | the DRM render node path, for the dma-buf paths |
| `sub_apis_` | `uint32_t` | `RVK_EXECUTOR_SUB_API_*` bits this GPU can serve |
| `import_kinds_` | `uint32_t[3]` | the `RVK_BLOB_*` mask per sub-API, because research §3 measured different answers per API on the same GPU |

Constructed by `HostGpuSet` while it enumerates, on the main loop, before any
context exists. Never mutated afterwards. Destroyed with the `HostGpuSet`.

Every operation is a read, and reads happen on both threads: `import_kinds` on
the main loop, `create` on a worker. A frozen-after-construction object needs no
lock, and that is the only reason it is safe.

## `HostGpuSet` (system)

Every host GPU this process found, and the lookup from a UUID to one of them.

| State | Type | Meaning |
|---|---|---|
| `gpus_` | `HostGpu[max_gpus]` | the GPUs enumerated, in Vulkan's order |
| `count_` | `uint32_t` | how many are live |
| `enumerated_` | `bool` | whether `initialize` has run |

Constructed with the `ExecutorLibrary`. `initialize()` runs the enumeration and
returns `Status` - it loads the Vulkan loader, enumerates physical devices,
probes each one's EGL and OpenCL reachability, and asks each API which
`RVK_BLOB_*` kinds it can import. Two-phase because all of that is fallible and
there are no exceptions (CLAUDE.md, *Construction is two-phase where it must
be*).

**When.** On the first `sub_apis` call, which the device makes exactly once, on
the main loop, during realize (`src/device/module/virtio_gpu_rvk.cpp:46`). It is
never re-run: the capset is frozen from that answer, and
`DeviceCapset::describe` returns without describing anything when the answer is
zero (`src/device/capset/device_capset.cpp:19`), so a library that cannot
answer without a GPU would disable the whole rvk protocol.

Destroyed at `dlclose` or process exit. **The ABI has no teardown entry point**
and the device's own comment says the table outlives the device
(`src/device/execution/executor_library.hpp:1-2`), so nothing in the device's
lifetime destroys it.

## `ExecutorLibrary` (system, library side)

What sits behind `RvkExecutorOps::opaque` and answers the table for this
process.

| State | Type | Meaning |
|---|---|---|
| `gpus_` | `HostGpuSet` | by value; the set is this object's |
| `sessions_` | `ObjectStorage<ContextSession>[max_sessions]` | the live sessions, the same shape the device uses for its own fixed tables (`src/device/context/device_context_table.hpp:51`) |
| `live_` | `uint32_t` | how many sessions are constructed |
| `initialized_` | `bool` | whether `initialize` has run |

There is exactly one, with static lifetime, constructed on the first call
through the table. The `RvkExecutorOps` itself is a file-scope `const` object -
the ABI requires a stable address, and the echo library does the same
(`tests/qemu/echo_executor.cpp`, its `const RvkExecutorOps table`). The
`extern "C"` functions are the constitution's ABI boundary and each is one line
into this object.

This is process-lifetime state and there is no way for it not to be: the ABI's
only entry point is a function with no argument. It is named here so that the
review does not have to discover it.

`create` runs on a worker and takes a session slot; `sub_apis` and
`import_kinds` run on the main loop and only read `gpus_`. Taking a slot is the
one place two threads could meet - two contexts' workers creating at once - and
the slot array is therefore claimed under the library's own mutex, which is the
only lock in the library.

## `ContextSession` (object)

One rvk context, from `create` to `destroy`. This is what an `RvkExecutor *`
actually points at.

| State | Type | Meaning |
|---|---|---|
| `sub_api_` | `uint32_t` | which of the three it is; decided at `create` and never again |
| `gpu_` | `const HostGpu &` | the GPU the UUID named, injected at construction |
| `backend_` | `ObjectStorage<VulkanBackend>` / `<OpenGlBackend>` / `<OpenClBackend>` | exactly one of the three is constructed |
| `arena_` | `DecodeArena` | the block one decode unflattens into |
| `blobs_` | `ImportTable` | this session's imports |
| `syncs_` | `ObjectStorage<SyncTokens>` | constructed only for an OpenGL session |
| `faulted_` | `bool` | whether this session has answered `RVK_DEVICE_LOST` |
| `executed_` | `uint64_t` | commands run, for the log and the suite |

Constructed by `ExecutorLibrary::create`, on the context's worker, into a slot;
`initialize()` allocates the arena block from the host and brings the backend
up, and a failure destroys the slot again so `create` can answer NULL rather
than a broken executor - which contract A5 requires.

Destroyed by `ExecutorLibrary::destroy`, on the same worker, after the device
has stopped the ring and will make no further call (contract §B3). Destruction
releases every import still held, because §B2 says so: *"`destroy` releases
everything still held, so a guest that never unrefs leaks nothing."*

Everything it does is on its worker. Nothing it owns is reachable from another
session, which is what makes acceptance scenario 1.3 - two contexts, neither
observing the other - a structural property rather than a test.

## `DecodeArena` (object)

The room one command's decode is allowed, and how often it was not enough.

| State | Type | Meaning |
|---|---|---|
| `block_` | `uint8_t *` | one allocation from the host ops, taken at `initialize` |
| `bytes_` | `size_t` | its size, `arena_bytes` |
| `arena_` | `WireArena` | the mark-and-reset allocator the generated decoders take (`src/proto/wire/wire_arena.hpp:21`) |
| `refusals_` | `uint64_t` | how many allocations `WireArena` answered null for |

Constructed with its session, destroyed with it, used only on the worker. The
block is taken once, at `create`: nothing on the submission path allocates.

Each `execute` resets it before decoding, so releasing every allocation of the
previous command is `reset()` and nothing else (`wire_arena.hpp:39-40`).

The refusal count is why this is a class and not a bare `WireArena`. Research §2
found that `WireArena::claim` answering null and a genuine protocol violation
both surface as `dispatch_command` returning `false`
(`src/proto/wire/wire_arena.cpp:8-28`), so a legitimate large command and an
attack are indistinguishable to the caller. `WireArena` gains the count; the
wire answer stays `RVK_UNDECODABLE` for both, and the suite asserts on the
count to tell them apart.

## `VulkanBackend` (object)

| State | Type | Meaning |
|---|---|---|
| `instance_`, `physical_`, `device_` | `VkInstance`, `VkPhysicalDevice`, `VkDevice` | one context's own instance and device |
| `queues_` | `VkQueue[max_queues]` | the queues it created |
| `lost_` | `bool` | whether a host call has returned `VK_ERROR_DEVICE_LOST` |

Derives from `DefaultServerDispatch` (`src/proto/generated/vulkan/vk_default_dispatch.h:15`),
not from `ServerDispatch` (`vk_dispatch_server.h:16`), so that widening the
profile is a behaviour change rather than a build break (research §2). It
overrides the 36 reserved ids and inherits the rest.

Constructed and destroyed with its session, on the worker. Vulkan has no thread
affinity of its own, but the device gives one thread per context anyway
(`src/device/execution/context_executor.hpp:1-3`), so no backend takes a lock.

## `OpenGlBackend` (object)

| State | Type | Meaning |
|---|---|---|
| `display_`, `context_` | `EGLDisplay`, `EGLContext` | surfaceless EGL, made current on the worker and never moved |
| `gl_` | `RealGl` | the generated POD table of 1375 function pointers, filled by `load_real_gl` (`src/proto/generated/opengl/gl_real_dispatch.h:1409`) |
| `region_` | `GuestRegion` | what `gl_.rvk_blob` resolves against |
| `lost_` | `bool` | whether `glGetGraphicsResetStatus` has reported a reset |

Unlike the other two there is no class to derive from: OpenGL's server half is a
free function over the table (`gl_dispatch_server.h:22`). This backend therefore
*owns* the table rather than being one, and supplies the three things the
generated code needs before it links at all:

- `gl_.rvk_blob` and `gl_.rvk_blob_user` (`gl_real_dispatch.h:25-26`), consulted
  at **1016** call sites in `gl_dispatch_server.cpp` - null there means every
  one of them refuses;
- the sync hooks the generated code calls through `proto/gl_sync_table.hpp`
  (included at `gl_dispatch_server.cpp:12`, called at `:1772`, `:3521`,
  `:4337`, and six more sites), which forward into `SyncTokens`;
- the four mapping commands the generator declined to guess.

EGL context currency is thread-affine, which is the strongest reason the worker
rule exists at all: this backend is made current on the worker inside `create`
and released inside `destroy`, both on the same thread.

## `OpenClBackend` (object)

| State | Type | Meaning |
|---|---|---|
| `platform_`, `device_`, `context_` | `cl_platform_id`, `cl_device_id`, `cl_context` | one context's CL objects |
| `queue_` | `cl_command_queue` | its queue |
| `lost_` | `bool` | whether a call has returned `CL_DEVICE_NOT_AVAILABLE` or worse |

Derives from `RealDispatch` (`src/proto/generated/opencl/cl_real_dispatch.h:26`),
which already implements all 105 entry points over the ICD loader, and overrides
the one command that is hand-written. Constructed and destroyed with its
session, on the worker.

## `ImportedBlob` (object)

One guest blob as one host API object.

| State | Type | Meaning |
|---|---|---|
| `blob_id_` | `uint64_t` | the id the guest used, which is the only name `execute` will see |
| `handle_` | `RvkBlobHandle` | a copy of the record the device handed over: kind, pointer or fd, size |
| `object_` | `uint64_t` | the API object it became - a `VkDeviceMemory`, a GL memory object name, a `cl_mem` - held as an integer because the ABI has no C++ type and the three are not one type |
| `owns_fd_` | `bool` | true when the kind is dma-buf and the import succeeded, which is when this object must `close` |
| `released_` | `bool` | set by `release_blob`, which makes a second call a no-op |

Constructed by `ImportTable::import`, on the worker. Destroyed either by
`release_blob` for its id, or by `destroy` sweeping what is left - both on the
worker.

An `RVK_BLOB_HOST_POINTER` handle's pointer is **never owned**: the device keeps
it mapped until `release_blob` or `destroy` returns. An `RVK_BLOB_DMA_BUF`
handle's fd *is* owned, but only on success - on failure the caller still owns
it - which is why `owns_fd_` is a member and not derived from the kind.

## `ImportTable` (system, library side)

One session's imports, by blob id.

| State | Type | Meaning |
|---|---|---|
| `slots_` | `ObjectStorage<ImportedBlob>[max_imports]` | the imports this session holds |
| `count_` | `uint32_t` | how many are live |
| `backend_` | a reference to the session's backend | what an import must be turned into |

Constructed and destroyed with its session; every operation on the worker.
Lookup is a linear scan over a fixed array: `max_imports` is small, there is no
heap for anything better, and nothing on the submission path may allocate.

A full table answers `Status::no_space`, which becomes `RVK_UNDECODABLE` on the
wire. That is a policy the guest can drive, which is why `max_imports` is a
number this document has to justify rather than assume.

## `GuestRegion` (object)

The base and length one context's blob resolver may hand out.

| State | Type | Meaning |
|---|---|---|
| `base_` | `uint8_t *` | the start of the one mapping bulk data may name |
| `length_` | `uint64_t` | its length |

Constructed with the OpenGL backend, re-pointed when a blob is imported or
released, read at every one of the 1016 `rvk_blob` sites. Its whole job is one
bounds check: an `(offset, bytes)` pair that does not lie wholly inside
`[base_, base_ + length_)` answers null, and the generated site refuses.
Everything it is asked about comes from the guest, so the arithmetic must not
wrap.

**A tension worth stating rather than hiding.** A guest-backed blob is not
necessarily one range: `GuestPageMapping` holds up to 16384 extents
(`src/device/memory/guest_page_mapping.hpp:23`), and neither a host pointer nor
a single dma-buf can express a scatter list (research §3). A `GuestRegion` of
one base and one length therefore serves a host-visible blob, or a guest-backed
blob that mapped as one extent, and nothing else. A scattered blob is a blob
this resolver refuses, which is a fault and not a crash.

## `SyncTokens` (system)

The live token-to-`GLsync` pairs of one OpenGL context.

| State | Type | Meaning |
|---|---|---|
| `tokens_` | `uint64_t[max_sync_tokens]` | the guest's token for each live pair |
| `syncs_` | `void *[max_sync_tokens]` | the `GLsync` it names |
| `count_` | `uint32_t` | how many pairs are live |

Constructed with an OpenGL session, destroyed with it, used only on the worker.
The three operations are the ones the generated code calls: `sync_for(token)`
answers the host fence or null, `bind_sync(token, sync)` records the pair
`glFenceSync` just made, `forget_sync(token)` drops it at `glDeleteSync`.

A token the guest invents resolves to null, and the generated site passes null
to the host call, which is a GL error rather than a fault. A table that is full
refuses `bind_sync`, and the pair is simply not recorded - the `GLsync` is
leaked into the context and freed when the context is destroyed. Both are
guest-drivable and both are bounded.

## `BlobHandleSource` (system, device side)

The device half of the memory channel: it builds the `RvkBlobHandle` the
executor is given.

| State | Type | Meaning |
|---|---|---|
| `host_` | `Host &` | the host ops, injected |
| `handles_` | `RvkBlobHandle[ImportTable::max_blobs]` | the handle built for each blob, cached |
| `built_` | a bitmap over the same range | which entries are real |

Constructed with the device, destroyed with it. **Every operation is on the main
loop**, and that is forced by the host ops, not chosen: `map_guest`
(`src/device/host/host.hpp:64`), `create_host_memory` (`:69`) and
`import_guest_memory` (`:96`) are all documented main-loop-only, while `execute`
runs on a worker. So the device builds the handle when the blob is created or
bound, caches it, and the worker passes the cached record to `import_blob`
(contract §B3: the device has *"built the handle on the main loop and cached it;
pinned the blob so it cannot be unmapped while imported; checked `size` is a
whole number of pages"*).

A host-visible blob becomes an `RVK_BLOB_HOST_POINTER` handle over the block's
own pointer. A guest-backed blob becomes an `RVK_BLOB_DMA_BUF` handle through
`/dev/udmabuf`'s `UDMABUF_CREATE_LIST`, which is the only one of the two that
can carry more than one extent. A blob that can be neither gets no handle, and
the executor is told to copy.

## `ExecutorConformance` (system, test side)

The scripted calls one library must answer, and the judgement of its answers.

| State | Type | Meaning |
|---|---|---|
| `ops_` | `const RvkExecutorOps *` | the table under test, from any library |
| `executors_` | `RvkExecutor *[max_conformance_contexts]` | the contexts the script created |
| `deadline_` | `uint64_t` | the watchdog bound one call may take |
| `failures_` | a fixed table of (statement, what happened) | what to print |

Constructed per run, destroyed at the end of it, and it is the one entity that
is allowed to use a second thread: contract A9 - *"Every call returns"* - is
only checkable by making the call under a watchdog, and A9 is one of the two
statements the device cannot enforce for itself. It runs against the echo
library (`tests/qemu/echo_executor.cpp`) and against this feature's library
alike; that it passes for both is SC-001.

---

## Ownership and lifetime

```
ExecutorLibrary (static, process lifetime)
├── HostGpuSet                 built once on the main loop, frozen
│   └── HostGpu[]              read from both threads, never mutated
└── ContextSession[]           one per rvk context, worker-only
    ├── VulkanBackend | OpenGlBackend | OpenClBackend
    │                          exactly one; OpenGlBackend owns GuestRegion
    ├── DecodeArena            one host block, taken at create
    ├── ImportTable
    │   └── ImportedBlob[]     each owns its fd, never its pointer
    └── SyncTokens             OpenGL sessions only
```

Order, outermost first: the library outlives every session; a session outlives
its backend, its arena and its imports; an `ImportedBlob` outlives nothing.

### What the session must not outlive, and what it does

A `ContextSession` holds **no pointer into anything the device owns** except for
the duration of one call. Two statements make that concrete:

- **The payload is borrowed for exactly one `execute`.** It points into the one
  staging block the device keeps per context, and the next batch overwrites it
  (`src/device/execution/batch_executor.cpp:57,62`). Contract A8 says the
  executor does not retain it past the call, and the suite checks it by
  overwriting the block immediately afterwards. The same holds for `reply`,
  which is null with a capacity of zero for every command that did not ask for
  one (`batch_executor.cpp:94-96`).
- **The session is destroyed *after* the staging block is freed, not before.**
  `DeviceContext` declares `executor_` before `staging_`
  (`src/device/context/device_context.hpp:55-56`), so destruction - reverse
  declaration order - frees the staging block first and only then lets
  `~ContextExecutor` release an executor the worker never destroyed
  (`src/device/execution/context_executor.cpp:11-14`). On the normal path the
  worker calls `destroy` while the block is still alive; on the teardown path it
  does not. Either way a retained payload pointer would be dangling by the time
  `destroy` ran, which is the second reason A8 is not advice.

An `RvkBlobHandle`'s **pointer** is a different lifetime and a longer one: it
stays mapped until `release_blob` or `destroy` returns (§B2), which is what
`VK_EXT_external_memory_host` requires, since it needs the pointer to stay
mapped for the life of the `VkDeviceMemory` (research §3). The handle *record*
itself is borrowed like the payload - `import_blob` takes a
`const RvkBlobHandle *` and `ImportedBlob` copies it.

## Validation rules

Quoted from `contracts/executor_abi.md` where the contract states them, so that
a change to the contract shows up here as a quotation that no longer matches.

| # | Rule | Source | Who enforces it |
|---|---|---|---|
| V1 | `size` is *"bytes, a whole number of host pages"* | §B1 | the device *"checked `size` is a whole number of pages"* (§B3) before `import_blob`; the executor checks it again, because §B3 is a promise and Principle V does not take promises |
| V2 | `kind` is *"exactly one RVK_BLOB_* value"* | §B1 | the executor: zero bits, both bits, or an unknown bit answers `RVK_UNDECODABLE` |
| V3 | `fd` is *"`RVK_BLOB_DMA_BUF`; -1 otherwise"* | §B1 | the executor: an fd that is not -1 on a host-pointer handle is a malformed handle, not a hint |
| V4 | `pointer` is *"`RVK_BLOB_HOST_POINTER`; NULL otherwise"* | §B1 | the executor, symmetrically |
| V5 | `reserved` is *"0"* | §B1 | the executor; a non-zero reserved field is how a version 3 handle would look to a version 2 reader |
| V6 | `import_kinds` *"answers a mask of `RVK_BLOB_*` bits, or zero for 'this sub-API on this GPU imports nothing; copy at the boundary'"* | §B2 | the library: zero is an answer, not a failure, and the capset must then say the blob is copied |
| V7 | `import_blob` *"answers `RVK_EXECUTED` or `RVK_UNDECODABLE`"*, and *"On success the callee owns an `RVK_BLOB_DMA_BUF` handle's `fd` and must close it; on failure the caller still owns it"* | §B2 | the library; the suite checks the fd is closed on success and still open on failure |
| V8 | `release_blob` *"is idempotent for an id that was never imported"* | §B2 | `ImportTable::release`: an unknown id, and an id already released, are both a no-op |
| V9 | An answer longer than `reply_capacity` *"writes nothing and sets `*reply_size` to the length needed"* | §A7 | the library; the device turns that into a reply header whose status is `status_too_large` (`src/proto/submission/reply_header.hpp:18`) |
| V10 | *"Arena exhaustion answers `RVK_UNDECODABLE`"*, and the two cases are *"distinguishable in the log and in `DecodeArena`'s refusal count"* | §C | `DecodeArena`; the suite asserts on the count |
| V11 | *"A result outside the defined set is not invented"* | §A10 | the library; the device today folds anything above 3 into a device-lost fault (`src/device/execution/context_executor.cpp:46-51` answering `invalid_state`, then `batch_executor.cpp:102` turning that into `FaultCode::device_lost`), which research §1 decided to fix in this feature |
| V12 | The command id's *"top 16 bits remain the API, and the device filters to one API per context before calling"* | §D | the device (`batch_executor.cpp:88-91`); a session that sees another API's id answers `RVK_UNKNOWN` rather than trusting the filter |

Everything in that table is attacker-chosen except the two the device promises,
and the two the device promises are checked anyway.

## State transitions

### `ContextSession`

```
      create ok              first fault              destroy
absent ────────▶ running ───────────────▶ faulted ───────────▶ absent
   ▲                 │                        │                   │
   └─────────────────┴────────────────────────┴───────────────────┘
        create fails: no session, create answers NULL (A5)
```

| From | To | What causes it |
|---|---|---|
| absent | running | `create` with a sub-API the library advertised and a UUID `HostGpuSet` knows; the backend came up and the arena block was taken |
| absent | absent | `create` failed: an unknown UUID, a sub-API this GPU does not serve, no host memory for the arena. The slot is released and `create` answers NULL, never a handle that faults later (A5) |
| running | running | `execute` answering `RVK_EXECUTED` or `RVK_UNKNOWN`; `import_blob`; `release_blob` |
| running | running | `execute` answering `RVK_UNDECODABLE` - a decode failure or an arena refusal. The session is untouched; **the device** stops the ring with fault code 6, and no further call arrives |
| running | faulted | `execute`, `import_blob` or any host call discovering the device is gone: `lost_` is set on the backend, `faulted_` on the session, and the answer is `RVK_DEVICE_LOST` |
| faulted | faulted | any further `execute` answers `RVK_DEVICE_LOST` again without touching the host API. §C requires a library whose host device is shared across contexts to answer it *"for each affected executor as that executor is next used, not once"*, and this is that behaviour |
| running or faulted | absent | `destroy`, after the device has stopped the ring (§B3): every import still held is released, the backend is torn down, the arena block is returned, the slot is freed |

`RVK_UNDECODABLE` deliberately does not move the session. The session cannot
know whether the device will fault the context, and a session that had torn
itself down would have nothing to answer with if it did not.

### `ImportedBlob`

```
        import_blob ok           release_blob        slot reused
absent ────────────────▶ imported ───────────▶ released ──────────▶ absent
   ▲                          │                                        │
   └──── import_blob fails ───┘        destroy sweeps whatever is left ─┘
```

| From | To | What causes it |
|---|---|---|
| absent | imported | `import_blob` with a valid handle the backend could turn into an API object. A dma-buf's fd is now owned and will be closed |
| absent | absent | `import_blob` failed a validation rule (V1-V5), or the host API refused the import. The answer is `RVK_UNDECODABLE`; the caller still owns the fd; nothing is recorded |
| imported | released | `release_blob` for that id: the API object is destroyed, an owned fd is closed, the pointer is simply forgotten because it was never owned |
| released | absent | the slot is reused by a later import |
| imported | absent | `destroy` sweeping what is left (§B2) |

**A re-import of a live id does not release the old one.** Research §3 rejected
release-implied-by-re-import: *"it makes 'same id, new backing' a silent
teardown, which is what a hostile guest will drive at the worst moment."* So
`import_blob` for an id already in the `imported` state answers
`RVK_UNDECODABLE` and changes nothing - the existing import keeps its object and
its fd, and the new handle's fd stays the caller's, exactly as any other failed
import (V7). The guest's way to rebind an id is `release_blob` then
`import_blob`, which is two calls because it is two decisions.

## Fixed bounds

There is no heap in the library beyond what the host ops give, and nothing on
the submission path may allocate, so every collection has a compile-time
maximum. One of the five below already exists in the tree; four are policy this
feature sets.

| Constant | Value | Where | Reasoning |
|---|---|---|---|
| `ExecutorLibrary::max_sessions` | **64** | **found, not proposed** | `DeviceContextTable::max_contexts` is 64 (`src/device/context/device_context_table.hpp:25`) and the device publishes it in the capset (`src/device/capset/device_capset.cpp:23`), so the device can never ask for a 65th executor; a second, different number would be a second cap nobody enforces |
| `ImportTable::max_imports` | **256** | proposed here, settled in tasks | The device holds at most 1024 blobs (`src/device/memory/blob_table.hpp:24`), so 256 lets four contexts each import a disjoint quarter of everything the device can hold, while keeping the fixed table at 64 × 256 entries instead of 64 × 1024 |
| `SyncTokens::max_sync_tokens` | **256** | proposed here, settled in tasks | A `GLsync` lives from `glFenceSync` to `glDeleteSync` and a guest that never deletes must be bounded; 256 outstanding fences is far past any real pipeline depth, and a pair is 16 bytes |
| `DecodeArena::arena_bytes` | **1 MiB** | proposed here, settled in tasks | Sized from the cost, not the wire: 64 live sessions is 64 MiB of host memory at full occupancy, and a command whose indirect data exceeds 1 MiB is refused as `RVK_UNDECODABLE` with the refusal counted, which is the signal that says raise it |
| `HostGpuSet::max_gpus` | **8** | proposed here, settled in tasks | This machine has two; 8 is the smallest round number that is not a limit anyone will meet, and a `HostGpu` is under 100 bytes |

Two notes for whoever settles them.

The arena number has no derivation available. Research §2 found there is no
largest command and no derivable size, because every count that sizes an
allocation comes off the wire; the only two `WireArena` constructions in the
tree are tests that disagree - 4096 bytes
(`tests/unit/proto/generated/test_generated_vulkan.cpp:27`) and 2048
(`tests/fuzz/fuzz_generated_decode.cpp:24`). The upper bound that does exist is
useless: a batch payload is at most the ring's data area, 16 MiB at
`RingHeader::highest_order` (`src/proto/submission/ring_header.hpp:18`), which
is also `BatchStaging::max_reply` (`src/device/execution/batch_staging.hpp:21`).
Sizing every session's arena for the worst possible ring would cost a gigabyte.

`max_imports` being smaller than the device's blob cap means a guest can make
`import_blob` fail by importing 257 blobs into one context. That is a fault, not
a crash, and the conformance suite should drive it deliberately.

## What is deliberately absent

| Absent | Why |
|---|---|
| An offset in `RvkBlobHandle` | The device hands over an exact range: for a dma-buf that is `udmabuf_create.offset` at creation, for a pointer it is `base + offset`, page-aligned. *"OpenCL's import has no offset property at all, so an offset in the ABI would have bought a capability flag and a branch per API for nothing"* (§B1, research §3) |
| A third handle kind | Two kinds exist because two have real users, measured on both GPUs in this machine. 039's exported-GPU-memory blobs would be a third, and nothing hands such a blob to a context yet, so it is not in the ABI (research §3; constitution VIII) |
| Asynchronous completion | *"The ABI gains no way to volunteer an answer, no fence parameter and no asynchronous completion"* (§D). Every call returns, which is what lets `destroy` and reset wait on the worker; an asynchronous executor would need a completion channel the device does not have |
| A per-command reply | One reply per batch, the last reply-flagged command's answer (`src/device/execution/batch_executor.cpp:107-108`), and `reply` is null with capacity zero for every command that did not ask (`:94-96`). An executor cannot volunteer an answer |
| A device-wide loss path | `RVK_DEVICE_LOST` is per executor. The device has no device-wide loss path and a qtest asserts other contexts survive; a library must answer it per affected executor as each is next used (§C) |
| A teardown entry point on the table | The table outlives the device (`src/device/execution/executor_library.hpp:1-2`). Adding one would be a third lifetime nothing needs |
| A worker identity in the ABI | `TableHost::require_worker` can only test that the big lock is not held (`src/device/abi/table_host.cpp:25-28`). Research §1 decided a real check needs a thread handle the host ops do not carry, and recorded it as a known gap in the suite rather than pretending it is enforced |
