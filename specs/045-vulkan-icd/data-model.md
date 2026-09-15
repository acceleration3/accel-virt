# Data model: Vulkan - the ICD on rvkgpu

## Guest user mode (`src/icd/vulkan`)

**IcdRoot** - one per process, in `ObjectStorage` in the exports' translation unit.
- Owns: the injected `Kmt`, `IcdMemory`, `Lock` factory, `LogSink`; the
  `EntryTable`; the `HandleRegistry`; the pool of `Connection`s.
- Rule: nothing is opened before the first `vkCreateInstance`.

**Connection** - one per `VkInstance`.
- Owns: the rvkgpu adapter handle and LUID, `GuestConnection`, `KmtTransport`
  (ring order 20, command limit 256 KiB, reply 64 KiB), a lock, a `faulted` flag.
- States: `opening` -> `open` -> (`faulted`) -> `closed`. A transport failure moves
  it to `faulted`, after which every call answers `VK_ERROR_DEVICE_LOST`.

**LoaderHandle** - one per dispatchable object.
- Fields: `loader_data` (first pointer-sized field, `ICD_LOADER_MAGIC` when
  created), `host` (the u64 wire handle), `connection`, `kind` (instance,
  physical device, device, queue, command buffer), `parent` (the handle it was made
  through), and for a command buffer its `CommandBatch`.
- Rules: `handle_to_wire` answers `host`; destroying a device destroys its queues'
  and command buffers' handles; a handle this ICD did not make is never
  dereferenced (the registry answers whether it made it).

**CommandBatch** - one per command buffer.
- Owns: framed commands in `IcdMemory` chunks, the count, a `recording` flag.
- States: `idle` -> (`vkBeginCommandBuffer`) `recording` -> (`vkEndCommandBuffer`)
  shipped -> `idle`. A begin while recording drops what was recorded.

**MemoryTypeTable** - one per physical device.
- Fields: the host's types with `HOST_VISIBLE` kept only in the host-pointer
  import mask; heap sizes from `D3DKMTQueryVideoMemoryInfo`; for each type whether
  it is served by chunks.

**HostVisibleChunk** - one per `host_visible_memory` allocation.
- Fields: allocation handle, lock view, size, `resource_id` from the bind escape,
  free-range list; chunk size 64 MiB (an allocation larger than a chunk gets a
  chunk of its own size).
- States: `allocated` -> `resident` -> `locked` -> `bound` -> `released`.

**ChunkAllocator** - one per device: the chunks and their placements; a placement
answers `(chunk, offset)`; a failed chunk allocation answers
`VK_ERROR_OUT_OF_DEVICE_MEMORY`.

**ExtensionPolicy** - the rows `{name, spec_version, test}`; answers the
intersection with the host's list and the generated allowlist.

**SlicedWait** - `deadline` (performance counter), `slice` (500 ms); answers the next
slice's timeout in nanoseconds, or that the deadline passed.

## KMD (`src/core`)

**AllocationKind::host_visible_memory** - pinned in the aperture segment,
`AccessedPhysically`, page count unbounded by the transport's limits.

**HostVisibleBindRecord** (`src/proto/resources`) - the 32-byte escape record of
contracts/host_visible_memory.md §1; decode refuses a wrong magic, version,
operation or non-zero reserved field.

**HostVisibleBinding** - one per bound chunk, on its rvk context's slot.
- Fields: the `Allocation`, the resource id, the chunk slot index (0-63).
- Rules: a context's teardown unbinds every chunk before its own resources; an
  allocation that is bound cannot be freed without unbinding.

## Device (`src/device`)

**BlobImportCommand** - the transport command handler for ids 2 and 3; validates
resource, range and alignment, answers the 16-byte reply.

**GuestPageAlias** - one per imported GUEST blob, in its `BlobResource`.
- Fields: the reserved host range, its length, the runs mapped into it.
- Rule: built on the context's worker, under the blob table's lock, when the
  first import names the blob; released after the last import holding it. A
  blob whose alias is held is not unreferenced (as built, T012: a worker cannot
  wait on the main loop, which joins it on `CTX_DESTROY`).

**BlobImports** - one per context: its imports (handle, executor blob id,
resource), at most 256, in host memory the first import allocates.

**ContextResources** (`src/proto/context`) - a context's resource ids from its
ring: 3 transport resources, then 64 chunk slots; shared by the KMD and device.

## Executor (`src/device/executor`)

**ImportedBlob** (exists) gains the requested memory type and the registered wire
handle; `VulkanBackend` routes `vkFreeMemory` of a registered import to `abandon`.
