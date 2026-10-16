# Host memory

Every byte that both the guest CPU and a host GPU touch is host GPU memory.
Each piece is its own allocation, made by one source per host GPU: the
executor's compositing Vulkan device. The source allocates nothing until a
guest asks.

- QEMU maps the allocation into the device's hostmem window.
- The guest kernel maps that part of the window into the process that asked.
- The host API the process's context runs on imports it.

Guest RAM is never handed to a host GPU. Nothing in the stack uses
`/dev/udmabuf`.

Memory the guest CPU never maps stays with each host API, as on real hardware:
device-local Vulkan memory, Direct3D default heaps, and OpenGL and OpenCL
objects. DXVK, vkd3d-proton, the host OpenGL driver and the host OpenCL
driver each allocate those themselves.

## Why

Before this, a host-visible chunk was guest RAM or a memfd, and every host
API had to import pages it did not allocate. Each driver had its own rules:

- RADV takes no host pointer over shared pages.
- NVIDIA takes no dma-buf into host-visible memory.
- udmabuf caps a dma-buf at 64 MiB and 1024 pages by default.

A Direct3D 12 arena or a large Vulkan allocation failed on one GPU or another.

Memory a GPU's own driver allocated and exported as a dma-buf imports back
into any device of that driver on that GPU. It maps on the CPU, and it is
coherent both ways. That was measured on the RX 9070 XT, Radeon 610M, Arc
(Meteor Lake), Iris Xe and RTX 3070 Ti, for both memory classes below.

## The pieces

### `HostMemorySource` (executor, one per host GPU)

`src/executor/host_memory_source.hpp`.

- **Allocation.** It allocates on the GPU's `CompositorDevice`, which the
  library opens with its GPUs; nothing is allocated before a guest asks. It
  allocates `VkDeviceMemory` with `VkExportMemoryAllocateInfo` (dma-buf) and
  exports it once.
- **Where the memory goes.**
  - QEMU maps the dma-buf into the window.
  - The source keeps the export to duplicate for importers.
  - The source maps the memory itself, for APIs that copy through it.
- **Probe.** When it opens, it tries every host-visible memory type with one
  small allocation, an export and a CPU map of the export. Only the types that
  pass are ever offered. A Vulkan guest sees the others without
  `HOST_VISIBLE` (see "The Vulkan guest's memory types"). It also asks the driver which types it would import each
  type's dma-buf as: an `upload` or `readback` request is answered first with a
  type that comes back host-visible, because vkd3d-proton imports a heap only
  as a type the driver reports. NVIDIA reports only device-local types for a
  dma-buf of its BAR (`DEVICE_LOCAL|HOST_VISIBLE`), which left every Direct3D
  12 upload heap refused.
- **Lifetime.** An allocation is counted: the blob holds one reference and
  every import holds one. The memory goes back to the driver when the last
  reference goes.

### The memory a guest asks for

Each request names a class:

| Class | Used for | Host memory type | Mapped |
|---|---|---|---|
| `upload` | Written by the CPU, read by the GPU: Direct3D 12 upload heaps, Direct3D 11 buffer slices | `DEVICE_LOCAL\|HOST_VISIBLE` (resizable BAR), else `HOST_VISIBLE\|HOST_COHERENT` uncached | write-combined |
| `readback` | Read by the CPU: readback heaps, query results, and every transfer arena the host CPU also reads | `HOST_VISIBLE\|HOST_COHERENT\|HOST_CACHED`, else `HOST_VISIBLE\|HOST_COHERENT` | write-back if cached, else write-combined |
| `vulkan(n)` | A Vulkan `vkAllocateMemory` of a host-visible type | exactly the host type behind the guest's type `n` | write-back if `HOST_CACHED`, else write-combined |

**The guest maps with the host's caching.** The device answers MAP_BLOB with
`MAP_CACHE_CACHED` or `MAP_CACHE_WC`, as the host maps the memory. The guest
kernel maps the process's view the same way. Mismatched cache types over the
same pages are an aliasing the SDM leaves undefined, and on a GPU that does not
snoop they read stale data.

- On AMD, KVM always honours the guest's PAT.
- On Intel, KVM ignores the guest's PAT unless QEMU disables
  `KVM_X86_QUIRK_IGNORE_GUEST_PAT` (`target/i386/kvm/kvm.c`), which KVM allows
  only on CPUs that self-snoop; both Intel hosts measured do. QEMU 11.1 leaves
  its `honor-guest-pat` at `off`, so `qemu-build.patch` makes it `auto`.
  Without it a guest's write-combined view is write-back: on Meteor Lake its
  writes stay in the CPU's caches while the GPU reads, or writes, the memory
  underneath.

**The source's own mapping is the driver's to choose.** It maps with
`vkMapMemory`, and ANV maps every exported allocation write-combined on a GPU
that shares the CPU's last-level cache, a `HOST_CACHED` type included: on the
Iris Xe the readback arena reads at 1 MiB in 26.7 ms through it, against 0.2 ms
of private memory, while the guest maps it write-back. Whatever the executor's
host drivers copy into the guest's memory through it is write-combined, and x86
does not order a write-combined store before a later write-back one. So the
device fences its stores (`Host::fence_stores`) after a batch's last command
and before its answer, head and fence: without it the guest took an OpenCL
rectangle read's answer while the rows were still in the host core's buffers,
and read what the arena held before.

### One allocation per request

A Vulkan allocation, a Direct3D 12 heap and an API's arena are each one
allocation of exactly their size. The guest no longer sub-allocates chunks:

- `ChunkAllocator`, `ChunkSpace` and `ChunkPlacement` are gone.
- `D3d12HeapArena` and `D3d12ArenaHeaps` are gone.

This is what native drivers and Venus do: an allocation the guest frees is
memory the host gets back at once, and the guest's handle is the host's
allocation with no offset to translate.

## The path of one request

1. **The process asks.** It sends `HostWindowRecord` `open` on its av context:
   bytes, and a class (`HostWindowChunk::open`).
2. **The kernel places the chunk.** It takes the first fit of window pages
   (`HostWindow`) and sends `RESOURCE_CREATE_BLOB`:
   - memory `HOST3D`, flag `MAPPABLE`;
   - `ctx_id` the context;
   - `blob_id` the encoded class;
   - a resource id the adapter allocates.
3. **The device gets the memory.** It asks the context plane for an allocation
   (`ContextPlane::allocate_memory`, then `AvExecutorOps::allocate_memory`).
   The answer is a dma-buf fd, its size and its caching. In renderer mode the
   fd comes back over the renderer socket.
4. **The memory is mapped.** The kernel sends `RESOURCE_MAP_BLOB` at the
   chosen offset. QEMU maps the fd there with `MAP_FIXED`, and the device
   answers with the caching. The kernel maps the pages into the process with
   that caching, and answers the resource, the bytes granted and the address.
5. **The context imports it.** The process sends `IMPORT_BLOB` with the
   resource id. The device checks that the blob was made for this context,
   then calls the executor's `import_blob(blob_id, memory_key)`, the key being
   the resource id the memory was allocated under.
   The executor answers the wire handle the guest names it by: for Vulkan the
   `VkDeviceMemory` the backend imported, for Direct3D 12 the heap.
6. **Release.** A `close` unmaps the process's view, then sends UNMAP_BLOB and
   UNREF. Only when the device answers do the window pages return to the
   bitmap. The device frees its reference, and the executor frees the memory
   once no import holds it.

### When the window cannot serve

Nothing falls back to guest RAM. An `open` the window cannot hold fails:
- there is no window;
- the window is full;
- the device or the executor refuses;
- a 32-bit process's view would end above 4 GiB.

What happens next depends on the API:
- **Arenas** (Direct3D, OpenGL, OpenCL) halve their request, down to 16 MiB.
  With none at all, bulk data crosses inline in the ring. Inline is not
  bounded - a command longer than its room crosses in pieces and an answer
  longer than the slot comes back in parts (docs/protocol.md, "Long
  commands") - but every piece is a batch and a copy, so bulk data that can
  be cut is cut into commands that fit:
  - OpenGL buffer data (`glBufferData`, `glBufferSubData`,
    `glGetBufferSubData` and their named spellings) goes in pieces each
    within the room (`BufferPieces`).
  - An OpenGL pixel transfer too large for its command goes through a host
    pixel buffer the context keeps (`PixelStagingBuffer`): the command names
    offset 0 of it bound at `GL_PIXEL_UNPACK_BUFFER` or `GL_PIXEL_PACK_BUFFER`,
    the host applies the pixel store there, and the bytes cross to it or
    from it in pieces.
  - OpenCL transfers are cut the same way (`ClTransferChunks`).
- **Direct3D 11 slices**: the guest copies mapped buffers instead.
- **Vulkan** answers `VK_ERROR_OUT_OF_DEVICE_MEMORY`, as a full GPU would.
- **A Direct3D 12 CPU-visible heap** answers `E_OUTOFMEMORY`.

## Per API

| API | Requests | Host import |
|---|---|---|
| Vulkan | `vulkan(n)` per `vkAllocateMemory` of a guest type the source backs; every other type, device-local memory included, is the host's own `vkAllocateMemory` | the dma-buf, as the guest's `VkDeviceMemory`, of the host type behind `n` |
| Direct3D 12 | `upload`/`readback` per CPU-visible heap; `readback` for the session arena | `vkd3d_proton_open_heap_from_dmabuf`, one heap per allocation |
| Direct3D 11 | `readback` session arena; `upload` slice storage, grown by allocation | arena: the source's mapping; slices: the dma-buf as a `VkBuffer` on DXVK's device, bound with `BindBufferStorage` |
| Direct3D 9 | `readback` session arena | the source's mapping |
| OpenGL | `readback` context arena | the source's mapping; readbacks through a memory object over the dma-buf where the driver takes one |
| OpenCL | `readback` context arena; `ALLOC_HOST_PTR` buffers in it | `CL_MEM_USE_HOST_PTR` over the source's mapping |
| Application memory (all of the above that take an application's pages) | `readback`, laid under the process's own pages | the API's own import of that blob |

## Application memory

Some APIs hand the driver memory the application already has and expect the
GPU to read and write those very pages:

- Direct3D 12's existing heaps (`OpenExistingHeapFromAddress`,
  `OpenExistingHeapFromFileMapping`), and every write-back heap, since
  D3D12Core asks for `D3D12_HEAP_FLAG_ALLOW_WRITE_WATCH` as nothing but a
  write-back heap and only memory the process allocated with
  `MEM_WRITE_WATCH` answers `GetWriteWatch` (GFXReconstruct's capture makes
  every upload buffer one);
- OpenCL's `CL_MEM_USE_HOST_PTR`;
- Vulkan's `VK_EXT_external_memory_host`;
- OpenGL's `GL_AMD_pinned_memory`.

Guest RAM is still never handed to a host GPU. Instead the pages become host
GPU memory where they are: the device lays a source allocation under them.

1. **The process lends the pages.** It sends `HostWindowRecord` `adopt` on
   its av context: the address and bytes (whole pages) and a class, always
   `readback` - the process maps its own pages write-back, so the host memory
   under them must be cached (`HostWindowChunk::adopt`,
   `HostAllocation::adopt`, `HostChannel::adopt_memory`).
2. **The kernel locks them.** `Platform::lock_process_pages` probes and
   locks the range for writing in the calling process (copy-on-write and
   demand-zero pages become its own), and a host blob id is taken. Nothing of
   the window is used and nothing is mapped (`AvContextTable::adopt_pages`).
3. **The device lays memory under them.** The kernel sends
   `RESOURCE_CREATE_BLOB` with memory `HOST3D_GUEST`, no flags, no entries,
   the class as the context object, then the pages' frames in order as
   `RESOURCE_ATTACH_BACKING` pieces of at most 2048 pages
   (`HostVisibleBlob::create_over`). The device allocates the memory as for
   any host blob and refuses memory the host does not map write-back
   (`HostBlobTable::create_over_guest`). For each run of a piece it copies
   what the RAM holds into the allocation at the run's offset and maps the
   dma-buf over the run's RAM in QEMU's address space with `MAP_FIXED`
   (`GuestPageOverlay`, `Host::overlay_guest_run`); KVM follows through its
   MMU notifiers, as it does for the hostmem window. The guest's page tables
   do not change: the process's pointer, its write watch and every other view
   of those frames now reach the host GPU's memory.
4. **The context imports it** with `IMPORT_BLOB`, as any host blob: the API
   names the import's handle in its commands.
5. **Release.** The close has the device release the blob: each run gets its
   RAM back holding what the allocation held (`Host::restore_guest_run` maps
   the RAM's own file there again after copying), so what the GPU wrote stays
   in the process's pages. Only then does the kernel unlock them - whatever
   the device answered, since a process may not end holding locked pages.

**Memory is given back only after the host's work over it is done.** An
API whose objects can be released with work still queued over them keeps
the memory out of use until it knows that work finished: OpenCL retires a
released buffer's arena block and its lent pages (`ClArenaBlocks::retire`,
`HostChannel::retire_memory`, both on `RetireList`) and reclaims them when
every queue of the context has drained; a queue released with work
unfinished holds them until the context ends. Direct3D 12 needs no list: an
application may not destroy a heap the GPU still uses.

A page lies under one blob at a time (`LentPages`, one set per device): a
second overlay would copy the first blob's memory into its own, and the
first's restore would take the page from under it. Two of a process's ranges
sharing a page - OpenCL buffers over neighbouring bytes - are told apart
there, and the second lending is refused; the API then uses its own copy as
before.

Limits:

- Guest RAM must be file-backed (`memfd`, as LSW's VM is): anonymous RAM
  could not be given back, so it is refused before anything moves.
- Each run is a mapping of the QEMU process (`vm.max_map_count`); one lending
  is at most 1 GiB (`AvContextTable::max_adopted_bytes`).
- A run is swapped by copy-then-map, so a write another of the process's
  threads makes to those pages in that instant can be lost; the API calls
  that lend and release are the moments an application does not write them.
- The device in av-renderer's process cannot overlay QEMU's RAM: there the
  lending is refused and the APIs copy.

## The Vulkan guest's memory types

The guest does not see the GPU's memory types as they are. One view per GPU
(`GuestMemoryTypes`, owned by its `HostMemorySource`) decides what it sees,
and the executor's Vulkan backend shows it to every context of that GPU:

- A type the source backs keeps its flags. Any other type loses
  `HOST_VISIBLE`, and `HOST_COHERENT` and `HOST_CACHED` with it.
- A heap with no type that is `DEVICE_LOCAL` alone gains one, if the source
  backs a type on it that is device local and host accessible and nothing
  else. The new type is never derived from a `PROTECTED` type.
- The types are sorted into the order the spec requires: no type's flags are
  a strict subset of an earlier type's. Otherwise the host's order is kept.

So a guest index is not a host index. The backend translates every index and
every `memoryTypeBits` that crosses:

- the memory properties, in all three spellings;
- `vkAllocateMemory`, imports included; an index the guest was never shown
  is refused with `VK_ERROR_OUT_OF_DEVICE_MEMORY`, since no driver checks one;
- every memory-requirements query, by object and by create info;
- `vkGetMemoryFdPropertiesKHR`.

The source translates `vulkan(n)` by the same view.

An image not made with the source's handle type is not offered the types the
source backs, unless nothing else would do. A driver that will not import an
image from a dma-buf keeps it compressed, and compressed memory is not memory
the guest should map. Buffers keep every type.

### Why device-local memory never goes through the source on a UMA GPU

On Intel's integrated GPUs (ANV on Meteor Lake and Iris Xe) every usable type
is `DEVICE_LOCAL|HOST_VISIBLE`. Without a device-local type of its own, every
guest allocation was source memory: render targets and swapchain images too.
Each one took window space, was mapped into the guest process, and was
imported.

ANV compresses an optimal image even in imported memory. It keeps the
compression metadata (CCS) inside the image's own binding. In the VM that
metadata was corrupted at cache-line granularity: 16 KiB tiles of garbage in
17 of 32 Vulkan samples on the Arc. With `INTEL_DEBUG=noccs` on QEMU every
picture was correct.

Memory the guest CPU never maps is the host API's own allocation, as on real
hardware and as on AMD and NVIDIA, which have a device-local type of their
own.

## What else stopped using udmabuf

- **Display scanout.** A head shows a guest blob as a display surface over the
  device's own mapping of its pages, instead of a dma-buf. This works on every
  display backend, GL or not.
- **Present snapshots.** A staged frame reaches a staging allocation's pages
  by the present engine's one CPU copy.

## Budgets

The adapter reports the host GPU's local memory to VidMm as a second segment
(`LocalMemorySegment`), so DXGI's `DedicatedVideoMemory` and the local budget
describe the host GPU. Nothing is ever placed in it. Vulkan's heaps are the
host's own.
