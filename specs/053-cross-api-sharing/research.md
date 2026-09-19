# Research: Sharing across APIs and processes (053)

Every answer below is read off this repository, the WDK headers, or the Khronos
registries already vendored here. Where a question could not be answered
without a Windows guest or a host GPU, that is said, with the layer that will
answer it.

## §1 — What crosses a process boundary, and what cannot

`D3DKMTShareObjects` makes an NT handle for a resource, a synchronization
object or one of a few other kinds; `D3DKMTOpenResourceFromNtHandle` turns it
back into per-process allocation handles in the opening process. What the
miniport sees of that is documented in kb 04, "Allocation lifetime traps":

> `DxgkDdiOpenAllocation` is called in *each* process that opens a shared
> resource, and the per-process driver data is separate from the
> per-allocation data. A remoting design must key host-side objects off the
> global allocation identity, not the per-open handle.

So the only thing that reliably reaches the second process is **the allocation
private driver data the creator wrote**. Everything else - the `hAllocation`
value, the device, the process - is different on the two sides.

**Decision.** The identity is a 64-bit id the miniport mints at
`DxgkDdiCreateAllocation` and writes into a `SharedObjectRecord` that follows
the `AllocationDescriptor` in the private data. `DxgkDdiOpenAllocation` in the
second process decodes that record and asks the adapter's `SharedObjectTable`
for the object.

**Alternative rejected.** Minting the id in the ICD and passing it out of band.
FR-001 forbids an out-of-band channel, and an id the kernel never saw is one
the kernel cannot refuse - which is exactly US1 scenario 4.

## §2 — Why the id must be refusable

A hostile process can call `D3DKMTOpenResourceFromNtHandle` with any handle
value, and can write anything into the private data of an allocation it creates
itself. Two refusals follow:

- an id the adapter's table never minted is refused (`Status::not_found`), and
  nothing on the host is touched;
- an id whose recorded kind, size or adapter LUID disagrees with the object the
  table holds is refused, because a guest that can make an image be opened as a
  fence is a guest that can make the host read a `VkSemaphore` as memory.

The LUID check is also the multi-GPU edge case: a handle from another adapter
carries another adapter's LUID, so the open fails and cross-adapter sharing is
never claimed.

## §3 — One host resource, two host devices

A guest context is a host context, and a guest device is a host device
(docs/testing.md, L4, "two devices sharing memory and a semaphore"). So "one
host object" cannot mean one `VkDeviceMemory`: two `VkDevice`s cannot share
one. It means **one exported descriptor**, from which each importer makes its
own object over the same storage. 047 already does this inside one context with
`vkGetMemoryFdKHR` and `VkImportMemoryFdInfoKHR`; the whole change is moving
the descriptor from `HostExportedFds` (per context) into `HostSharedObjects`
(per device).

**Decision.** `HostSharedObjects` owns one fd per object id and hands every
importer a `dup` of it. The refcount is the number of contexts holding the
object, not the number of fds: a context that imports the same id twice holds
one reference, as `ImportTable` refuses a duplicate id today.

**Consequence for US1 scenario 3 (the owner exits).** The device's table holds
the fd, not the owning context, so the owner's `DeviceContext` being destroyed
drops the owner's reference and leaves the object alive for the others.

## §4 — Nothing new reaches the guest as a descriptor

047's `HostExportedFds` exists because a guest could otherwise name an fd it
made up (constitution V). Shared objects avoid the question: the guest names an
object by its 64-bit id and never sees a descriptor at all. The fd is created
inside the device by `export_object` and consumed inside the device by
`import_object`; it never appears in a wire body.

The hostile surface this feature adds is therefore exactly three things: the
`SharedObjectRecord` in allocation private data (miniport side), and the 24-byte
`SharedObjectRequest` body of the three transport commands (device side). Each
gets a hostile table and a fuzz target.

## §5 — Where the transport commands go

`docs/protocol.md`, "Transport commands", already defines API 0 with ids 1-3
(NOP, IMPORT_BLOB, RELEASE_BLOB), answered by the device itself whichever
executor library is loaded. Shared objects belong there for the same reason
blobs do: the device, not the library, owns the identity.

**Decision.** Ids `0x4` SHARE_OBJECT, `0x5` OPEN_SHARED_OBJECT, `0x6`
RELEASE_SHARED_OBJECT, all carrying the same 24-byte body, framed exactly as
`BlobImportCommand` frames its two: a body of any other length is command
framing (fault 5), and every other refusal is a status in the answer with the
context running on.

**Alternative rejected.** A control-queue request instead. The control queue is
the main loop under the BQL (kb 11, "Threading"), and the export has to happen
on the *owning context's worker*, because a host `VkDevice` is that worker's.

## §6 — What the executor ABI is missing

The table (`src/device/abi/rvk_executor.h`) has `import_blob`, whose handle is
guest pages, and `imported_handle`. Neither can move a host object between two
executors: a shared object has no guest pages at all, and the descriptor has to
come out of one executor and go into another.

**Decision.** ABI version 6 adds three calls, and changes nothing else:

- `export_object(executor, kind, wire_handle, int *fd)` - a descriptor for an
  object this executor's guest already made;
- `import_object(executor, kind, fd, size, uint64_t *wire_handle)` - that
  descriptor made into an object of this executor's, answered as the handle the
  guest will name it by;
- `release_object(executor, wire_handle)` - that object destroyed.

A library of version 5 or below answers every shared-object command with
"unsupported", which is a status, not a fault - the same shape `IMPORT_BLOB`
already has for an old library.

**Alternative rejected.** Extending `import_blob` with a new `RVK_BLOB_*` kind.
`import_blob` is keyed by `blob_id` and its handle names guest memory; a shared
object is neither, and the overload would make every existing branch read two
ways.

## §7 — What each host API imports a descriptor as

| Guest API | Export | Import |
|---|---|---|
| Vulkan memory | `vkGetMemoryFdKHR`, opaque fd | `VkImportMemoryFdInfoKHR` in `vkAllocateMemory` |
| Vulkan semaphore | `vkGetSemaphoreFdKHR`, opaque fd | `vkImportSemaphoreFdKHR` |
| OpenGL memory | - (nothing in GL exports a descriptor) | `glImportMemoryFdEXT` on a `glCreateMemoryObjectsEXT` name |
| OpenGL semaphore | - | `glImportSemaphoreFdEXT` on a `glGenSemaphoresEXT` name |
| OpenCL memory | - (no export in `cl_khr_external_memory`) | `CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR` property on `clCreateBufferWithProperties` |

Only Vulkan exports. That is not a gap in this design: the spec's matrix is
Vulkan→GL, GL→CL and CL→Vulkan, and in every real application the *allocator*
is the API that has an exportable allocation. An `export_object` a backend
cannot serve answers "unsupported", and the guest's export then fails with that
API's documented error, which is the "formats or sizes one API cannot
represent" edge case generalised.

GL and CL both import a **dma-buf**, not an opaque fd. Vulkan exports either.
So a shared object records which kind its descriptor is, and an importer that
cannot take that kind is refused - which is why `SharedObjectRecord` and
`HostSharedObject` both carry the kind and neither infers it.

## §8 — What the OpenGL generator is missing

`cl_khr_gl_sharing`'s commands are already in `tools/codegen/profiles/opencl.toml`
with committed ids (`clCreateFromGLBuffer` … `clEnqueueReleaseGLObjects`), so
US2 scenario 3 needs no generator work at all.

OpenGL's external-object commands are not. The import itself does not need one -
the device makes the memory object inside `import_object` and answers its name,
exactly as `IMPORT_BLOB` answers a `VkDeviceMemory` - but *using* it does:

```
glBufferStorageMemEXT, glTextureStorageMem2DEXT,
glDeleteMemoryObjectsEXT, glDeleteSemaphoresEXT,
glSignalSemaphoreEXT, glWaitSemaphoreEXT
```

Six commands, every parameter a scalar or a counted array of scalars, which is
shapes the emitter already lowers (`glDeleteBuffers` is the same shape as
`glDeleteMemoryObjectsEXT`). They go in `opengl.toml`'s `extra` with
append-only ids.

`glImportMemoryWin32HandleEXT` is deliberately **not** added. Its handle
parameter is an application's `HANDLE`, and putting one on the wire would hand
the device a value only the guest kernel can resolve; the ICD resolves it to an
object id first and the device never sees a handle.

## §9 — Where the NT handle lives in the ICD

047's `ExternalObjects` turns an exported handle back into the `VkDeviceMemory`
it names, which works inside one process and nowhere else. The handle must now
be a kernel object, and all three ICDs need the same translation, so it is one
class in `src/icd/common/`:

`icd::SharedHandles` maps a Win32 handle to the object id behind it, minting
one through `Kmt::share_object` on export and resolving one through
`Kmt::open_shared_object` on import. `ExternalObjects` keeps its job - which
`VkDeviceMemory` or `VkSemaphore` a handle names *in this process* - and gains
a second question it can answer: which object id, for a handle it did not
export.

**Three `Kmt` calls are added**, and `GdiKmt` translates each into one gdi32
function:

| `Kmt` | gdi32 |
|---|---|
| `share_object(device, allocation, handle)` | `D3DKMTShareObjects` |
| `open_shared_object(device, handle, allocation)` | `D3DKMTOpenResourceFromNtHandle` |
| `close_shared_object(handle)` | `CloseHandle` (kernel32) |

`MockKmt` scripts all three, including the refusals US1 scenario 4 needs.

## §10 — What this environment can and cannot prove

| Claim | Layer that proves it | Available here |
|---|---|---|
| the record and the bodies refuse hostile input | L1 + L3 | yes |
| the miniport mints, refuses and refcounts | L1 + L2 | yes |
| the device holds one host resource for two contexts | L1 + L4 | yes |
| an owner dying leaves the object alive | L1 + L4 | yes |
| the ICDs report the extensions only where the host has the fd ones | L1 | yes |
| two host devices really see the same pixels | L4 on a host GPU | only where the host imports (docs/testing.md) |
| two guest *processes* see the same pixels | L5 | no: no prepared guest image here |

SC-001 and SC-003 are L5 claims and are not demonstrated by this change; the
gate reports the guest as skipped, and the spec's US1 in-guest half stays open
until an image is available.
