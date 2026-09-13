# Research: the host executor

Three questions had to be answered from the code and the machine rather than
from the spec: what the device actually promises an executor, what the
generated server dispatch actually asks of a backend, and which zero-copy
import paths actually exist on real hardware. All three were answered by
reading the tree and by running probes on this host; nothing below is inferred
from documentation alone.

## 1. The device's side of the ABI, as the code has it

`src/device/abi/rvk_executor.h` is 63 lines and the whole contract:
`abi_version`, `opaque`, `sub_apis`, `create`, `destroy`, `execute`. The device
accepts a table only if the version is exactly 1 and none of the four function
pointers is null (`src/device/execution/executor_library.cpp:7-16`).

What the reading changed:

- **`sub_apis` is called once, on QEMU's main loop, during realize**
  (`src/device/module/virtio_gpu_rvk.cpp:46`), before any context and before any
  GPU work. Its answer is frozen into the capset for the device's lifetime, and
  `DeviceCapset::describe` returns without describing anything when the answer
  is zero - so an executor that cannot answer without a device would disable the
  whole rvk protocol rather than degrade.
- **One reply per batch.** `BatchExecutor::run_command` keeps only the last
  reply-flagged command's answer (`batch_executor.cpp:107-108`), and `reply` is
  null with capacity zero for every command that did not ask
  (`batch_executor.cpp:94-96`). An executor cannot volunteer an answer.
- **`RVK_DEVICE_LOST` is per context.** It destroys that executor and faults
  that ring; a qtest asserts every other context keeps running
  (`tests/qemu/test_virtio_gpu_execution.py:113-129`). There is no device-wide
  loss path and no way for a library to ask for one.
- **The payload is borrowed.** It points into one staging block per context that
  the next batch overwrites (`batch_executor.cpp:57`); it is valid only for the
  duration of the call, and nothing guarantees its alignment.
- **Nothing enforces "every call must return".** `Worker::stop` joins
  unconditionally on the main loop, so an `execute` that blocks forever hangs
  QEMU at `CTX_DESTROY`.
- **Nothing bounds what the library writes.** The device sizes the reply block
  to exactly `reply_capacity` and never checks; it believes `*reply_size`.

Three defects in the device that this feature owns, because this feature owns
the loading and validation (FR-001):

1. `rvk_executor_ops()` returning null is not checked
   (`src/device/qemu/virtio-gpu-rvk.c:65`), and `accept(nullptr)` is `ok`, so
   such a library yields a silently capset-less device instead of a failed
   realize.
2. `TableHost::require_worker` only tests that the BQL is not held
   (`src/device/abi/table_host.cpp:25-28`), so it cannot tell one worker from
   another, and a violation only logs.
3. A result outside 0..3 is folded into `device_lost`
   (`context_executor.cpp:50`), so a buggy library is indistinguishable from a
   lost device.

**Decision.** Fix 1 and 3 in this feature; leave 2, because a real
worker-identity check needs a thread handle the host ops do not carry, and the
mock already catches the case that matters in unit tests. Record 2 as a known
gap in the conformance suite rather than pretending it is enforced.

## 2. What a backend owes, per API

Read from the generated code, not the spec.

| | Vulkan | OpenCL | OpenGL |
|---|---|---|---|
| Shape | abstract `ServerDispatch`, 386 pure virtuals | abstract `ServerDispatch`, 105 | **no class**: `dispatch_command(const RealGl&, ...)` over 1375 function pointers |
| Passthrough given | `RealVulkanDispatch`, all 386, with lazy `vkGetInstanceProcAddr` | `RealDispatch`, all 105 | only `load_real_gl(RealGl&, GlLoader, void*)` |
| Safe base given | `DefaultServerDispatch` | none | none |
| Hand-written commands owed | 36 reserved ids (WSI, `vkGet*ProcAddr`, descriptor buffer) | 1 | 4: `glMapBuffer`, `glMapBufferRange`, `glUnmapBuffer`, `glFlushMappedBufferRange` |

**Decision.** Derive the Vulkan backend from `DefaultServerDispatch`, not from
`ServerDispatch`, so that widening the profile is a behaviour change rather than
a build break. Derive the OpenCL backend from `RealDispatch` for the same
reason. OpenGL gets neither and is therefore the phase that carries the risk.

Three things block `gl_dispatch_server.cpp` from compiling at all, and each is
this feature's:

- `proto/gl_sync_table.hpp` does not exist. Three functions are called at nine
  sites - `sync_for(token) -> void*`, `bind_sync(token, GLsync)`,
  `forget_sync(token)` - with no header and no weak default.
- `RealGl::rvk_blob` / `rvk_blob_user`, a bounds-checked
  `(user, offset, bytes) -> uint8_t*` resolver, is consulted at **1016** call
  sites. Null means every one of them refuses. This is OpenGL's bulk-data path,
  not a corner case; Vulkan and OpenCL have no equivalent because OpenCL copies
  bulk data into the arena instead.
- The four mapping commands above, which the generator declined to guess
  because a pointer into host GL memory cannot cross the wire.

**The arena, and a gap worth naming.** The generated decoders allocate every
indirect argument from a caller-owned `av::WireArena`, and every count that
sizes an allocation comes off the wire. There is therefore no largest command
and no derivable size; the only two `WireArena` constructions in the tree are
tests that disagree (4096 and 2048). Worse, `allocate` answering null and a
genuine protocol violation both surface as `dispatch_command` returning
`false`, so a legitimate large command and an attack are indistinguishable to
the caller.

**Decision.** The executor owns one arena block per context, sized by a named
constant, and `WireArena` gains a count of the allocations it refused so the two
cases can be told apart in a log and in a test. The wire answer stays
`RVK_UNDECODABLE` either way - a frame that needs more room than this host will
give is a frame this host cannot take - but the operator and the suite can see
which happened. Changing `WireArena` is this project's to do; it is our type.

## 3. Zero-copy import, measured on this host

Probes were written and run against both GPUs in this machine: Intel Iris Xe
(Mesa 26.2.2) and NVIDIA RTX 3070 Ti (proprietary 580.178.04).

| Path | Intel | NVIDIA |
|---|---|---|
| Vulkan, host pointer (`VK_EXT_external_memory_host`) | aliases | **aliases** |
| Vulkan, dma-buf (`VK_KHR_external_memory_fd` + `VK_EXT_external_memory_dma_buf`) | works, host-visible type | works, **device-local only**; host-visible types answer `VK_ERROR_OUT_OF_DEVICE_MEMORY` |
| OpenGL, dma-buf (`GL_EXT_memory_object_fd`) | aliases | **fails**, `GL_OUT_OF_MEMORY` |
| OpenGL, host pointer (`GL_AMD_pinned_memory`) | aliases | **extension absent** |
| OpenCL, dma-buf (`cl_khr_external_memory`) | aliases | **absent**; its `opaque_fd` is a CUDA handle |
| OpenCL, `CL_MEM_USE_HOST_PTR` | unified memory, true alias | discrete; the spec's "may copy" is the honest reading |

`minImportedHostPointerAlignment` is 0x1000 on both, which the device's existing
4 KiB page granularity already satisfies. `/dev/udmabuf` is present and usable
by this user, and QEMU's own `UDMABUF_CREATE_LIST` path is in the tree we build
(`hw/display/virtio-gpu-udmabuf.c:30-67`), which matters because a guest-backed
blob is not one range: `GuestPageMapping` holds up to 16384 extents, and neither
a host pointer nor a single dma-buf can express a scatter list.

**The conclusion that shapes the ABI: no single identifier is zero-copy
everywhere.** Vulkan wants a host pointer and gets it on both GPUs; OpenGL and
OpenCL want a dma-buf and get it only on Mesa. Carrying one kind would give up a
whole API on one of the two GPUs in this machine.

### Decisions

- **Two handle kinds, tagged, not one.** `RvkBlobHandle` carries a kind, a
  pointer or an fd, and a size. Two kinds, two real users, no third invented:
  039's exported-GPU-memory blobs would be a third kind, and there is no caller
  for it, so it is not in the ABI (constitution VIII).
- **Offset is always zero; the device hands over an exact range.** For a dma-buf
  that is `udmabuf_create.offset` at creation; for a pointer it is `base +
  offset`, page-aligned. OpenCL's import has no offset property at all, so an
  ABI offset would have forced a capability flag and a per-API branch for
  nothing.
- **Import and release are explicit, and destroy releases whatever is left.**
  `VK_EXT_external_memory_host` requires the pointer to stay mapped for the life
  of the `VkDeviceMemory`, and a guest can unref a blob before its context dies,
  so without a release the device could hand Vulkan a dangling pointer.
- **The capability query is on the table, not on an executor**, and takes a
  sub-API and a GPU UUID. The capset is described at realize, before any context
  exists, so a per-executor query could not be answered in time.
- **The device builds the handle on the main loop and caches it.** Every memory
  operation in the host ops is documented main-loop-only, and `execute` runs on
  the worker. Import and release therefore join the worker rule, over a handle
  the device prepared earlier.
- **The fallback copy lives in the executor.** Only it knows the destination's
  shape - a `memoryTypeBits` intersection, a GL buffer, a `cl_mem` with a
  128-byte base alignment - so a device-side copy would land in a bounce buffer
  the executor then copies again. Measured cost: ~45 µs per MiB in RAM, which is
  survivable for compute buffers and fatal per frame, which is exactly why the
  capset bit has to be truthful.

### Alternatives considered

- **Host pointer only.** Simplest, no fd ownership, no `/dev/udmabuf`
  dependency. Rejected: it gives up OpenGL and OpenCL zero-copy on Mesa, where a
  dma-buf demonstrably works.
- **dma-buf only.** Best reach for GL and CL, and the device is already equipped
  for it. Rejected: it loses the NVIDIA Vulkan host-visible import, which is the
  single best path that exists on that GPU.
- **A `VkDeviceMemory` kind for 039's exported blobs.** Rejected as speculative:
  nothing hands such a blob to a context yet.
- **Offset carried in the ABI.** Rejected: expressible in Vulkan and OpenGL,
  impossible in OpenCL, so it buys a capability flag and a branch per API.
- **Release implied by re-importing the same id.** Rejected: it makes "same id,
  new backing" a silent teardown, which is what a hostile guest will drive at
  the worst moment.

## 4. What the library may be built with

`src/proto` and `src/device` have no C++ standard library, no exceptions and no
RTTI, and feature 063's T016 removed the last `std::vector` from the generated
server dispatch, so nothing forces the line to move.

**Decision: it does not move.** The executor library is built like every other
target here - freestanding, no STL, no exceptions, no RTTI, memory from the
host ops with an explicit pool. This settles feature 063's open question in
`contracts/generated_code.md`: the `server_dispatch` emitter may not have its
`std::vector` back.

One consequence to record rather than fix: the generated `ServerDispatch` bases
declare a **public virtual destructor**, which CLAUDE.md's kernel adaptation
forbids because a deleting destructor needs `operator delete`. Generated
directories are excluded from `check_design.py` by one named rule (063, T010),
and this feature's own backends declare no destructor and are never destroyed
through a base pointer - they live in `ObjectStorage`. The rule holds where it
is enforced; the exception is the generator's, and it is written down here.

## 5. Decisions taken after the Phase 1 review

Writing the quickstart found four places where the design did not say enough to
turn a success criterion into a runnable step. Each is settled here.

**The conformance suite does not need QEMU.** The `AV_QEMU` configuration
returns before the sanitizer flags are set, so a suite living there could never
satisfy SC-001's "under ASan and UBSan". It does not have to live there: an
executor library is an ordinary shared object, and the suite's job is to call
the ABI, not to run a guest. So the library builds in **every** configuration
and the suite is an ordinary ctest binary that loads it - sanitized by default,
covered by the coverage gate, fuzzed in the fuzz configuration. The qemu
configuration additionally points a real device at the same `.so` through the
`executor=` property, which is the integration half rather than the contract
half. Invocation: `executor_conformance <library path>`, run once per library,
and a suite that only this project's library passes has tested nothing.

**`import_kinds` reaches the guest through the capset bit that already means
it.** FR-007 wants the capset to report only the import paths that work.
`CapsetInfo` already carries feature 039's three probe bits, and
`feature_guest_memory_import` is exactly this claim; this feature makes it
truthful by setting it only when `import_kinds` answers non-zero for the bound
GPU. Per-sub-API granularity does **not** go on the wire: a guest whose sub-API
cannot import gets a copy, which is what the bit not being set would have told
it anyway, and a new bit per API is structure with no reader (constitution
VIII).

**SC-002 splits, because half of it is feature 044.** A Vulkan triangle, an
OpenGL clear and an OpenCL vector add are this feature's proof that the backends
work, and they run in the conformance suite: built with the generated
serializers, handed to the ABI directly, read back from the reply. Running the
same three *from a guest, through the device* needs the transport and the ICDs,
which are 043 and 045, and having them work end to end in a real guest is
literally what feature 044 is for. The spec's SC-002 is corrected to say which
half is which rather than promising a guest this feature does not have.

**The ABI fuzz target splits three ways.** SC-003 asks for 60 seconds per
sub-API; the fuzz gate gives 60 seconds per target. Three targets -
`fuzz_executor_vulkan`, `fuzz_executor_opengl`, `fuzz_executor_opencl` - each
with its own corpus, is the honest reading, and it also stops one API's easy
crashes from starving the other two of coverage.

## 6. Two more, found writing the data model

**The OpenGL sync hook gets a user pointer, because it cannot reach a session
without one.** `RealGl` already carries `rvk_blob` and `rvk_blob_user` for the
guest-memory resolver, but the generated sync hooks are parameterless free
functions - `av::glproto::sync_for(token)`, `bind_sync(token, GLsync)`,
`forget_sync(token)` - called at nine sites in `gl_dispatch_server.cpp`. A
parameterless function cannot find one context's `SyncTokens` without
thread-local or file-scope state, and this project has neither by rule.

The fix is in the generator, which is ours (063): the three hooks join
`rvk_blob` as fields on `RealGl`, with an `rvk_sync_user` beside
`rvk_blob_user`. The emitted code then reaches the session exactly the way the
1016 bulk-data sites already do, `proto/gl_sync_table.hpp` is a header this
project writes rather than a hole the linker finds, and the guest side's
`mint_sync_token` is unaffected because the guest mints, it does not resolve.
It is a wire-shape change to nothing: no id moves, no serialization changes.

**The library's root is a fourth process-lifetime object, and it has to be.**
`rvk_executor_ops()` takes no argument, so the table it answers is file-scope
`const` data and the one `ExecutorLibrary` behind it is file-scope storage -
the same shape as `MiniportBinding`'s pointer and the kernel shim's root, and
for the same reason: a C entry point with nowhere to put a `this`. It lives in
`ObjectStorage` in the single translation unit that defines the ABI functions,
and `CLAUDE.md`'s enumeration of process-lifetime state is amended to name it
rather than leaving a fourth one unlisted.

**Scattered guest blobs are refused, not stitched.** `GuestRegion` resolves one
range, and `GuestPageMapping` holds up to 16384 extents. For Vulkan and OpenCL
that does not matter - the device hands over one `UDMABUF_CREATE_LIST` dma-buf
and the scatter is the kernel's problem. It matters for OpenGL's `rvk_blob`,
which must answer a host pointer: a blob with more than one extent has none. The
resolver refuses, the command faults, and the capset bit that promised import
is the one that should not have been set. Stitching would mean a copy into the
arena, which is what OpenCL already does for bulk data, and it can be added
later against a measured need rather than a guessed one.
