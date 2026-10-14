# Plan: mapped buffers the host GPU reads in place

A design for review, not yet a plan to carry out. It says what is wrong, what
replaces it, which APIs it covers and in what order, and what is still unknown.
Where it and the code disagree, the code is right.

**Superseded in part by docs/host-memory.md.** Where this plan puts the
memory - a span of the session's blob arena, imported by host pointer or as a
dma-buf over its pages, or guest RAM where the window cannot hold a chunk - is
gone. Direct3D 11's slices live in a slice store of their own: 128 MiB of
`upload` host GPU memory per session, which the session opens and imports like
any host allocation and DXVK's device imports from its dma-buf.
`BindBufferSlice` names the store (`memory`) beside the slice's offset, and the
store's bookkeeping stays in the session's 32 MiB `readback` arena, which is
read cached. Direct3D 12's CPU-visible heaps are one host allocation each. The
renaming, retirement and map policies below stand.

## 1. The bug that started it

Subnautica (Unity 2019.4, Direct3D 11) drew "vertex explosions": whole objects
and terrain torn for a frame. Measured, step by step:

| Experiment | Result |
|---|---|
| Upload arena as a ring off | still exploding |
| Host `ConstantRing` bypassed | still exploding |
| Every mapped buffer sent whole at Unmap | no explosions |
| Twin copy of each watched shadow, compared page by page at Unmap | 4 to 95 pages changed per DISCARD map of a 647 KB vertex buffer that the write watch did not report; no NO_OVERWRITE map ever lost one |
| Shadow made read-only after Unmap, writes trapped | seven Unity worker threads write it 40 to 280 µs **after** the render thread's Unmap, from `UnityPlayer.dll` |
| Unmap waits 1 ms before reading the watch | 0 lost pages, 0 traps, no explosions |

Unity hands a mapped pointer to its job threads and unmaps before they are
done. That is outside the Direct3D 11 contract, and it works on every real
driver: the mapped pointer *is* the memory the GPU reads, and the GPU reads it
when the draw runs, long after the jobs finish. avumd hands out a private
copy instead and sends it at Unmap, so it has to guess when the application
is done writing. Here the guess was wrong; the late pages went out with the
next frame's DISCARD, into the next buffer, and the draws that needed them
read holes.

The full upload at Unmap only hid this: copying 650 KB took long enough for
the workers to finish. The 1 ms wait hides it the same way. Neither is a fix.

## 2. The behaviour every API needs

An application writes through a pointer, and the GPU reads what it wrote
when a later command runs. Every API that maps a buffer for writing asks for
that, and the guest drivers implement it in two different ways:

| API | Today | Guesses when the writes are done |
|---|---|---|
| Direct3D 11 | A private shadow; at Unmap the write-watched pages, or the whole buffer, are copied into the stream. Small dynamic constant buffers are packed again on the host (`ConstantRing`). | Yes: at Unmap |
| Direct3D 9 | A private shadow; Unlock sends the locked range | Yes: at Unlock |
| OpenGL | Private copies (`BufferShadows`); unmap sends the mapped range; a persistent map is sent again at flush points | Yes: at unmap and at each flush |
| Direct3D 12 | Upload and readback heaps, each a host allocation of its own the GPU reads in place | No |
| Vulkan | `HOST_VISIBLE` memory, each allocation a host allocation of its own | No |
| OpenCL | `ALLOC_HOST_PTR` buffers over the context's arena | No |

The copying three share the flaw this bug exposed; the other three already
work the way real hardware does. So the behaviours are not too different to
consolidate: they are one behaviour, implemented twice, and the copying
implementation is the one to retire.

What does differ between APIs is only the *policy* at a map:

| Intent | Direct3D 11 | Direct3D 9 | OpenGL |
|---|---|---|---|
| Fresh storage, the old contents left to draws already sent | `Map(WRITE_DISCARD)` | `Lock(D3DLOCK_DISCARD)` | `glMapBufferRange(MAP_INVALIDATE_BUFFER_BIT)`, `glBufferData` on a live buffer (orphaning) |
| Same storage, the application promises not to touch what is in use | `Map(WRITE_NO_OVERWRITE)` | `Lock(D3DLOCK_NOOVERWRITE)` | `MAP_UNSYNCHRONIZED_BIT`, persistent maps |
| Same storage, after the GPU is done with it | `Map(WRITE)`, `Map(READ_WRITE)` on a default buffer the CPU may write | `Lock(0)` | a plain map |

So the design is one guest component that owns the memory and the renaming,
and a thin map policy per API on top of it.

### Where the line between shared and per-API falls

| Layer | Shared or per API | Why |
|---|---|---|
| Window chunks, slices, size classes, retirement against the GPU serial, the wait when memory runs out | **Shared**: one class, `BufferSlices` | It is where reuse-too-early bugs live; three copies would be three chances to get it wrong. Direct3D 11, Direct3D 9 and OpenGL need exactly the same thing. |
| Which map flags mean fresh storage, the same storage, or a wait | **Per API**: a policy of a few dozen lines each | The flags and their rules differ (OpenGL's orphaning and persistent maps, Direct3D 9's lock ranges). One class over all of them would switch on the API. |
| Pointing the host's buffer at a slice | **Per host backend** | Direct3D 11 and Direct3D 9 both run on DXVK and share one path. OpenGL cannot change a buffer's storage and needs a name indirection of its own. |
| Direct3D 12, Vulkan, OpenCL | Use the chunks and slices, **not** the renaming or retirement | There the application reuses memory under its own fences; a driver retiring it behind the application's back would be wrong. |

`BufferSlices` starts in `src/icd/d3d/`, the portable core of avumd, where
Direct3D 11 and Direct3D 9 both draw from (`src/um/d3d/` is the Windows shim,
which has no unit tests), so it has two users from the start. It moves to
`src/icd/common/` when OpenGL, a separate binary, takes it up: the point at
which it has users in two drivers (CLAUDE.md, "Keep code near its owner").

## 3. The design

### 3.1 Guest: `BufferSlices`, one per session

A slice allocator over host memory (docs/host-memory.md), shared by every
buffer a session maps for writing:

- **Memory.** *Superseded:* the plan took a span of the session's blob arena,
  as Direct3D 12's heaps then were, with guest RAM where the window could not
  hold the arena. What was built is the slice store: one `upload` host
  allocation of its own, imported once, so a slice is an offset into memory
  both sides map, and nothing more is opened per slice. Without a window or
  room for the store, the session copies mapped buffers instead.
- **A buffer's storage is a slice**: the store and an offset. A buffer starts
  with one when it is created with CPU write access.
- **Renaming.** A map that asks for fresh storage takes a new slice and sends
  one command, `BindBufferSlice(resource, memory, offset, bytes)`, `memory`
  the store's import handle. The command carries no
  bytes. The old slice is retired, tagged with the GPU serial (3.3) of the
  last batch sent before the rename.
- **Reuse.** A retired slice is free again once the GPU serial the host
  published has passed its tag. Slices come in power-of-two size classes, with
  a free list for each class. A session that has no free slice and no room for
  a new chunk waits for the GPU serial with a call, the way DXVK waits when
  its own slices run out.
- **Unmap sends nothing.** The application's writes are already in the memory
  the GPU will read, whenever they land.

This replaces `WatchedShadows`, the write watch, and the Unmap uploads of
every mapped buffer it covers.

### 3.2 Host: the buffer over the slice

The executor imports the slice store once, as a `VkBuffer` on the device the
session runs on, from the dma-buf the memory source exported (this plan's
host-pointer and memfd routes are gone, docs/host-memory.md). A
`BindBufferSlice` then points the API's buffer at that memory:

- **Direct3D 11 and Direct3D 9 (both DXVK).** DXVK already has what is needed:
  `DxvkMemoryAllocator::importBufferResource` makes an allocation over an
  external `VkBuffer`, and `DxvkContext::invalidateBuffer` swaps a buffer's
  storage for one, which is how DXVK renames a buffer itself.
  `DxvkDevice::importBuffer` is used for the same purpose by D3D11On12.
  Neither is reachable through a COM interface, so a small patch to the
  vendored DXVK adds one interop method, used by both APIs: *bind this
  buffer to (VkBuffer, offset, host pointer)*, recorded on the CS thread like
  any other command. The buffer must also be pinned so that DXVK never
  relocates or renames it by itself (`DxvkContext::ensureBufferAddress`); the
  host never maps these buffers, so DXVK has no other reason to.
- **OpenGL.** A host GL buffer cannot change its storage. A buffer bound to
  slices would be a GL memory object over the imported chunk
  (`GL_EXT_memory_object_fd`) with a new GL buffer name per slice, and the
  executor's name table would map the guest's one name to the current GL
  name, rebinding wherever the old one was bound. That is real work in the GL
  backend, which is why OpenGL comes last (section 5).

### 3.3 The GPU serial the guest reuses slices by

Today the guest learns only when the host has *executed* a batch: the ring
head, and the transport fence. A slice can be reused only when the *GPU* is
done with it, which is later. The host already tracks GPU progress for
Direct3D 11 readbacks (`D3dBackend::await_gpu_unlocked`, a D3D11 fence per
session); this design publishes it:

- the host signals the session's fence after each batch it executes, with the
  batch's transport fence id as the value;
- it writes the completed value into a word of the session's first window
  chunk whenever it next looks: at each batch, and on the device's existing
  timer;
- a guest that needs a slice and finds none free first reads the word, and
  only then waits with a call that returns when the GPU reaches a value.

### 3.4 What it means for Unity's late writes

Unity's workers write into the slice after Unmap. The draw that reads it runs
on the host GPU after the batch is published, executed, and submitted by
DXVK. That is the same window real hardware gives, so the data is there when
the GPU reads it. Nothing on the guest has to guess any more.

## 4. What stays as it is

- **Copies the application asks for.** `UpdateSubresource`, `glBufferSubData`,
  texture uploads: the application hands over bytes at the call, and the copy
  at the call is correct by definition. They keep the upload arena.
- **Textures mapped for writing.** A DXVK image cannot sit over linear guest
  memory in general. Dynamic textures keep the shadow and send at Unmap, the
  whole subresource, as they do now. The late-write risk stays there; no case
  of it has been measured.
- **Readbacks.** A staging buffer mapped for reading is unchanged.
- **Direct3D 12, Vulkan and OpenCL** already work this way.

## 5. Order of work

Each step builds, passes its tests, and is usable on its own.

1. **The GPU serial (3.3)**, published and waited for. Unit tests for the
   guest's reading of it; an executor device test that the value reaches the
   word only after the GPU finished.
2. **`BufferSlices` (3.1)** with its size classes, retirement and reuse, unit
   tested against a fake serial: a slice is never handed out while its tag is
   ahead of the serial, and running out waits rather than reuses.
3. **The DXVK patch and the executor's `BindBufferSlice` (3.2)** for
   Direct3D 11. An executor device test: a buffer bound to a slice, a draw
   recorded, the slice written *after* the bind command, and the draw reads
   what was written; a DISCARD rename leaves an earlier draw reading the old
   slice.
4. **Direct3D 11 vertex, index and other dynamic buffers on slices.**
   `WatchedShadows` goes. Subnautica, Half-Life 2's intro (the arena's
   throughput case) and DWM's many small NO_OVERWRITE appends are the checks.
5. **Direct3D 11 dynamic constant buffers on slices** - done with step 4:
   they are dynamic buffers the CPU writes too. The host's `ConstantRing`
   stays, for a session whose host cannot draw from slices and copies.
6. **Direct3D 9** on the same component and the same DXVK method.
7. **OpenGL**, with the name indirection of 3.2.

## 6. Open questions

These have to be answered before or during the steps they belong to.

1. **Read speed of the memory.** On the Intel host the window is system RAM
   either way. On a discrete GPU, vertex fetch from system memory crosses PCIe
   on every draw that reads it. DXVK already places dynamic buffers in
   host-visible memory, so this should be no worse than native, but it has to
   be measured on a discrete card.
2. **Coherency - answered.** An Intel GPU (Meteor Lake, Mesa ANV) reads an
   imported dma-buf without snooping the CPU's caches: guest writes still in
   the cache are invisible to the draw. Measured with a standalone Vulkan
   program writing a fresh 4 KiB and copying it out, 200 times: 199 copies
   stale through a udmabuf, 0 through the same memory imported by host
   pointer (`VK_EXT_external_memory_host`). No memory type of either kind
   snoops. A write-combined guest mapping does not help either: KVM maps
   guest RAM write-back whatever the guest's page attributes say. *Superseded
   by docs/host-memory.md:* the memory is now the GPU driver's own allocation,
   exported and imported back, which is coherent both ways, and the guest maps
   it with the caching the host maps it with (`MAP_BLOB` answers cached or
   write-combined), so neither udmabuf nor a host-pointer import is used.
3. **DXVK's assumptions - answered.** `importBufferResource` took an
   allocation from the allocator's pool without its lock, which every other
   path to the pool holds; DXVK only ever called it once, at D3D11On12 setup.
   Called per bind from the embedder's thread, it raced DXVK's own frees for
   the pool's free list: allocations were handed out twice, DWM drew garbage,
   and one run crashed QEMU in that function. Patch 0003 now takes the lock.
4. **Deferred contexts.** A command list records a DISCARD map; each
   `ExecuteCommandList` replays it. The slice the recording wrote must live as
   long as the list, and a replay must bind it again rather than take a new
   one.
5. **Window budget.** Slices in flight per buffer are bounded by how far the
   GPU is behind. DXVK keeps a similar pool. Each session's store is 128 MiB of
   the window; whether Subnautica needs more should be measured in step 4.
6. **Until this lands**, the committed avumd still loses Unity's late writes.
   A stopgap - checking the watch again before the next draw and at the next
   map - would rely on Unity finishing its jobs before it issues the draw,
   which has not been measured.
