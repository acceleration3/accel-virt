# Shared surfaces: design

**Status: proposal, for review before any code.**

One host system class, `SharedSurfaces`, owns every surface the guest shares,
whatever API made it and whatever API reads it. Every API session and the
present engine go through it. This note says why one is missing, what it would
own, how readers and writers are ordered through it, and the order to build it
in.

It was written while making Portal's Direct3D 9 renderer show frames. Portal's
device goes through Direct3D 9 on 12 (D3D9On12) onto our Direct3D 12 driver.
Its frames are drawn on the host, then presented as *redirected blts*. A
redirected blt needs a reader, the present engine, to take pixels from a surface
another API wrote, in that API's order. Nothing on the host owns that today.
The facts below come from reading the code and from traces taken in the guest
on 2026-09-30 and 2026-10-01: DxgKrnl ETW, the UMD and KMD logs, and probes
under `tools/guest/probes/`.

## 1. What happens today

### 1.1 The guest already has one identity

The miniport mints one id for every shared allocation, in one space per adapter.
That covers Direct3D surfaces and the carriers Vulkan, OpenGL and OpenCL share
memory and fences through.

- **Minting.** `SharedObjectTable::mint` runs at `DxgkDdiCreateAllocation` when
  the private data is an `AllocationDescriptor` followed by a
  `SharedObjectRecord`.
- **Never reused.** Ids only increase while the adapter runs.
- **Handed on.** The creator learns its id at its own implicit
  `OpenAllocation`. Every process that opens the allocation is handed the same
  id.
- **Released.** The guest releases an id once, with `SharedReleaseRequest`
  (0x02F1), when the last process lets go. A virtio reset releases every id at
  once (id 0).

So "which surface" already has one answer in the guest: `docs/protocol.md`
(shared objects, 0x02F1) and `docs/kb/04-memory.md` ("key host-side objects off
the global allocation identity").

### 1.2 The host has three owners for it

The host has more than one owner for that id, and each is tied to one API:

| Owner | Holds | Scope | Who uses it |
|---|---|---|---|
| `D3dSharedResources` (in `D3d11HostDevice`) | key → `IUnknown*`, 4096 rows | per GPU, per process | Direct3D `CreateResource(shared_key)`, `OpenResource`, `ShareResource12`; scanout `export_image` / `read_image` |
| `InteropTextures` (in `D3d11HostDevice`) | a D3D12 texture remade on DXVK's device over vkd3d-proton's exported memory | per GPU | `ShareResource12` |
| `D3d12Presentation` | a shared `ID3D12Fence`, imported into DXVK as a timeline | per D3D12 session | `PresentResource12`: DXVK waits for the presenting queue |
| `SnapshotPool` (in `D3d11HostDevice`) | 16 staging textures, keyed by a *random* key the UMD chose | per GPU | D3D11 blt presents (`PresentSnapshot`); the present engine's `map_snapshot` |
| `HostImageExport` | one linear dma-buf image on DXVK's device | per GPU | scanout to QEMU's display |
| `HostSharedObjects` | fd descriptors, 256 | per device | feature 053: Vulkan exports; Vulkan, OpenGL and OpenCL import |
| `HostExportedFds`, `GlSharedObjects`, `ClSharedObjects` | what one context exported or imported | per context | Vulkan, OpenGL, OpenCL |

The two families do not meet. `D3dBackend::export_object` and `import_object`
refuse feature 053's descriptors, and Vulkan, OpenGL and OpenCL cannot open a
Direct3D key. No class owns its own Vulkan device: `DxvkVulkanDevice` borrows
DXVK's queue through its interop interface.

### 1.3 What that costs

- **D3D9On12's frames are black.** Each Direct3D 9 present reaches the D3D12
  UMD as a blt (`DXGI_DDI_PRESENT_FLAGS` 0x1), and dxgkrnl turns it into a
  `Blt|RedirectedBlt` into the window's redirection surface (ETW 184, then 166
  per present). The miniport can only copy the source's guest pages, and the
  frame was never there. The D3D11 answer is a UMD snapshot (`PresentSnapshot`
  under a random key), and it has two limits:
  - **It is D3D11-only.** The snapshot pool lives on DXVK's device.
  - **The UMD would have to know the present is a blt**, and the D3D12 UMD
    cannot: a DXGI flip-model buffer arrives the same way. In both, the present
    flags are Blt (0x21 vs 0x1, differing only in AllowFlexibleRefresh), the
    resource flags 0x11, the heap flags 0x26, `OptimizeForComposition` 1 and
    `VidPnSourceID` ~0.

  Only the reader knows: the present engine asks for a blt's source, while the
  compositor reads a flipped buffer itself.
- **A self-deadlock waits in mixed batches.**
  1. `D3dBackend::run` takes the D3D11 device's non-recursive mutex at a
     batch's first D3D11 command, and holds it to `end_batch`.
  2. A D3D12 command in the same batch runs under it.
  3. `ShareResource12` reaches `D3d11HostDevice::share_image`, which locks the
     same mutex.

  A process with a D3D11 and a D3D12 device - an overlay drawing into a D3D12
  game is enough - can hang its session on its first swap chain.
- **Fence kinds do not meet across APIs.** Vulkan exports fences as OPAQUE_FD,
  while OpenGL and OpenCL import only DMA_BUF descriptors, so OpenGL cannot open
  a Vulkan-shared fence.
- **Cross-GPU sharing does nothing.** A buffer made on a GPU other than DWM's
  is shared into that GPU's table, where DWM's `OpenResource` does not look
  (`d3d-desktop.md` §3.9).
- **Under the renderer, blts copy guest pages.** The renderer plane maps no
  snapshots, so capset bit 5 is off, and every blt-model present falls back to
  guest pages.
- **Vulkan and OpenGL swap chains present by read-back.** They read the image
  into guest memory and GDI-blit it. No shared id is involved, so they can't use
  the host-side paths at all.
- **Snapshot keys are trusted guesses.** A snapshot key is a 64-bit number the
  UMD picked. A process naming another's key is stopped only by having to guess
  it. The miniport knows the presented allocation's real id, and never uses it
  for a blt.

## 2. The proposal

### 2.1 Scope and ownership

`SharedSurfaces` is a system class. There is one per device, which is one
adapter and so one id space. It lives in whichever process runs the device's
contexts: QEMU's for the local plane, `av-renderer`'s for the isolated one.
The plane owns it beside `HostSharedObjects`, which it absorbs (§4, step 4).

It is injected into every API backend of every context of the device, and into
the plane's present-engine and scanout calls. It holds references to the GPU's
host devices: DXVK's, vkd3d-proton's, the native Vulkan instance and the EGL
display. It never makes them.

One per device, not one per GPU, because the id space is per adapter. Today's
per-GPU `D3dSharedResources` would mix two devices' ids if two devices were ever
bound to one GPU, and nothing stops that.

### 2.2 A surface

A row is one guest id:

- **id:** the miniport's, never chosen by the host or the UMD;
- **kind:** image, buffer or fence (feature 053's memory/fence kinds, plus the
  description an image needs);
- **description:** extent, format, mip and layer counts, sample count, tiling,
  and the formats it may be viewed as;
- **producer:** the API and session that made it, and the host object they made
  it as;
- **canonical memory:** an exportable allocation, as an fd. This is what every
  other API's view is made over. D3D12 already exports one (vkd3d-proton
  `0003-shared-image-fd`); D3D11 and Vulkan make shared resources exportable;
- **views:** the surface as each API that opened it sees it, made on first
  open and released with the row. `InteropTextures` becomes the D3D11 view
  maker, and the rest follow it;
- **points:** the last point written and the last one each reader finished
  (§2.4);
- **uses:** whether a blt has ever read it. This decides staging (§2.5).

### 2.3 One interface over the host APIs

Each host API is behind a `SurfaceApi`. This is a real interface with four
implementations: Direct3D 11 over DXVK, Direct3D 12 over vkd3d-proton,
Vulkan, and OpenGL. OpenCL reaches surfaces through its OpenGL vehicle, as it
does today. A `SurfaceApi` does five things:

1. **Export:** a host object it made, given as canonical memory and a description.
2. **View:** canonical memory made into its own object (texture, resource,
   image or memory object).
3. **Signal:** a point on a timeline, in its queue's order.
4. **Wait:** for a point on a timeline, in its queue's order.
5. **Stage:** a copy of a surface at a point, into staging memory a CPU or
   another device can read (§2.5).

`SharedSurfaces` decides; the `SurfaceApi`s only make the calls. That keeps the
deciding testable against a mock `SurfaceApi`, the way `MockPlatform` makes the
kernel testable.

### 2.4 Points: the order between writers and readers

A **point** is a value on a timeline: a timeline semaphore shared as an OPAQUE_FD,
which every API above can wait for and signal.

- **Timeline:** each writing session has one. The D3D12 session's is the shared
  fence `D3d12Presentation` makes today, generalized. D3D11 sessions share
  DXVK's one immediate context, so a D3D11 reader of a D3D11 writer needs no
  wait. A point is recorded anyway, for readers on other APIs.
- **Written:** when a writer publishes a surface's contents, it signals its
  timeline in its queue's order. A publish is a present, a D3D11 flush of a
  shared resource, or a Vulkan or OpenGL queue signal the application asked
  for. The surface records that point as its last written.
- **Read:** a reader waits for the written point before it reads, in its own
  queue's order. The reader is the compositor's D3D11 view, the present
  engine's copy, the scanout's export, or another API's view.

The guest names points where it must. A present names the point it published:
`PresentResource12` and the D3D11 present gain a point, and the private data
dxgkrnl carries to `DxgkDdiPresent` carries it too (§3). The miniport pairs that
point with the source allocation's **own** id, not a key the UMD picked.

Applications' own synchronization still works as it does now: Vulkan
semaphores, D3D12 shared fences, and in time `IDXGIKeyedMutex`. Points add the
order Windows gives implicitly, between a present and whoever shows it.

### 2.5 Readers

- **Compose (flip model).** DWM's `OpenResource(id)` gets a D3D11 view of the
  surface. Before each present, DXVK's queue waits for the point the present
  published. This is what `D3d12Presentation::present` does today, moved to its
  owner.
- **Scanout (a flipped primary).** `SCANOUT_IMAGE(id)` exports the surface for
  QEMU's display after its written point. `HostImageExport` becomes the scanout
  half of `SharedSurfaces`.
- **Blt (redirected or not).** The present engine asks `read(id, point)`, and
  three cases follow:
  - **A surface no blt has read before.** It is waited for at `point` and
    copied directly, then marked as blt-read. That first copy can race the
    writer's next frame, a frame at most.
  - **A surface marked blt-read.** It is staged eagerly at every point the
    writer publishes. `SurfaceApi::stage` copies it in the writer's own queue
    order, into a staging slot, before the writer's next work can touch it. The
    engine maps that slot. This is today's `SnapshotPool::take`, generalized to
    any writer: D3D11 copies to a staging texture, D3D12 to a readback buffer
    on its queue, Vulkan to a host-visible image.
  - **A flipped surface.** No blt ever asks for it, so no stage is ever made,
    and flip-model presents cost nothing.

  The writer never waits for a reader. The staging copy is ordered on the
  writer's own queue, so no back-edge runs from the present engine into a guest
  queue, and nothing can hang on a present that never comes.

Staging slots are `SharedSurfaces`' own, keyed by (id, point), with the
eviction `SnapshotPool` already has (oldest unmapped first). Under the
renderer, a slot's memory is a dma-buf or memfd passed to QEMU over the
existing `SCM_RIGHTS` channel, so isolated devices get blt presents without
read-back too.

### 2.6 Release, reset and host lifetimes

- **Release.** `SharedReleaseRequest(id)` releases the row. The row's views and
  canonical memory are freed once no reader holds them and the GPUs are done
  with them. `InteropTextures::collect` is the model.
- **Reset.** A reset (id 0) releases every row.
- **Destroying sessions.** A row outlives the session that made it, for as long
  as the guest holds the id. DWM opens surfaces whose creators are already gone,
  and `D3dSharedResources` holds them for exactly that reason; the row inherits
  the rule.
- **Capacity.** Fixed per device, with no allocation on the present path. The
  row count is today's 4096 plus the descriptor table's 256. The staging slot
  count must be more than the presents the engine holds outstanding.

### 2.7 Locks and threads

- **Its own lock.** `SharedSurfaces` has a lock of its own, and holds it only
  around its table.
- **No API calls under it.** It never calls a `SurfaceApi` while holding it.
  It decides under the lock, lets go, then calls. That removes §1.3's
  self-deadlock as a class, rather than one instance of it.
- **Where the API's work runs.** `SurfaceApi` calls run under the API's own
  rules: the D3D11 device lock for DXVK, none for vkd3d-proton, the session's
  thread for native Vulkan and OpenGL.
- **Which threads call it.** Session workers publish and open. The present
  engine's thread reads. QEMU's main loop exports scanouts and releases.

### 2.8 Processes

Everything a device's contexts share is already in one process: "One renderer
serves one device" (`renderer.md`). `SharedSurfaces` stays inside that boundary.
Only staging slots and scanout images cross it, as fds, the way scanout
dma-bufs already do.

## 3. Wire and ABI changes

- **Guest → host.** A present names `(point)`. `PresentResource12` gains it,
  the D3D11 present path replaces `PresentSnapshot` with it, and the
  present's private data carries `PresentPointRecord { point }` in place of
  `PresentSnapshotRecord { key }`. The miniport adds the source allocation's id
  itself.
- **Present engine.** A queued present's image source becomes `(id, point)`
  (0x02F7's `source_key` today, documented as "a presented surface's key", and
  that now becomes true).
- **Executor ABI.** It goes to version 14:
  - `read_surface(id, point)` / `release_read` replace `map_snapshot` /
    `unmap_snapshot`.
  - `export_image` and `read_image` stay, and resolve through `SharedSurfaces`.
- **Shared objects.** Feature 053's `SHARE_OBJECT` / `OPEN_SHARED_OBJECT` /
  `RELEASE_SHARED_OBJECT` stay on the wire, and resolve through the same rows.
  So does Direct3D's `CreateResource(shared_key)` / `OpenResource` /
  `ShareResource12`. Each family keeps its commands, and they meet in one
  table.

## 4. Migration, each step shippable and tested

0. **Done, uncommitted: D3D12 residency.** D3D12Core makes heaps resident
   through the driver's `MakeResident`, which answered without paging anything
   in. So a shown surface's kernel allocation was never resident, and dxgkrnl
   refused every blt that named it before the miniport saw it: one ETW Blit in
   a whole run. `D3d12Residency` pages heaps' kernel allocations on the
   runtime's queue. Now each present is one blt (123 of 123), and the frames
   are black instead of missing.
1. **`SharedSurfaces` with D3D11 and D3D12 behind `SurfaceApi`.** This
   replaces `D3dSharedResources`, `InteropTextures` and `D3d12Presentation`'s
   fence. Behaviour is unchanged, except that the self-deadlock goes.
2. **Points and blt reads.** This adds `read_surface` and `PresentPointRecord`,
   with staging for blt-read surfaces. D3D11 blts move onto it, and
   `SnapshotPool` and the random keys retire. **Portal's Direct3D 9 frames show
   from this step.**

   Done, as `map_frame` / `unmap_frame` (ABI 14), `PresentPoint` and
   `PresentResource12`'s point, and `D3d11FramePool` / `D3d12FramePool`
   staging into `SharedSurfaces`. Getting Portal's frames on screen also took:
   - a multisampled present resolved before it is read - Direct3D 9 on 12
     presents its antialiased back buffer as it is - into a texture of the
     frame's own, in a typed format (`DxgiResolveFormat`), at a single-sample
     placement;
   - texture copies judged by the region they touch (`D3d12Footprint`,
     `D3d12CopyExtent`), and the DDI's physical footprint taken as texels:
     every block-compressed upload was refused, which removed the device;
   - kernel surfaces sized in rows of blocks: D3D9On12 shares every texture,
     and a BC1 texture counted a block a texel asked for sixteen times its
     bytes, running the aperture out loading a level;
   - the drain marking each queue on its own fence (`D3d12QueueProgress::
     mark_all`), so an idle queue reaching a shared fence first no longer
     ends it early.
3. **Scanout.** `HostImageExport` and `read_image` move under `SharedSurfaces`.
4. **Feature 053 joins.** `HostSharedObjects` becomes the rows' descriptor
   store, and Vulkan, OpenGL and OpenCL open Direct3D surfaces and the reverse.
   Fence kinds are reconciled in one place (§1.3).

   Begun, Direct3D 11 to the others: a shared texture of one 2D mip, layer and
   sample (`ExportableImageInfo`) is made by `InteropTextures` over dedicated
   memory exported as a dma-buf, wrapped for DXVK, and the UMD publishes it
   with SHARE_OBJECT under the key the miniport minted, so OpenGL and Vulkan
   open it by that id like any shared object. OpenGL imports memory as
   *dedicated*, which is what makes radeonsi read RADV's layout from the
   dma-buf (`test_d3d_gl_interop_device`). Vulkan binds an image made from the
   same `ExportableImageInfo` - it lives in `proto/vulkan/` because both sides
   must describe one image - to memory imported as the Vulkan backend imports
   any, and RADV lays both out alike (`test_d3d_vulkan_interop_device`, which
   fails for a linear image). The reverse - Direct3D opening a Vulkan or
   OpenGL object - is still refused.
5. **Vulkan and OpenGL swap chains present surfaces.** They stop reading back
   into guest memory and GDI-blitting.

   OpenGL done: each window gets a DXGI flip chain on the context's own
   adapter (`icd::WindowChains`, `DxgiWindowChains` in the guest), whose two
   slots are such shared textures. A swap copies its frame into a free slot
   with glCopyImageSubData - a copy, so the application's scissor and masks
   cannot touch it, as they did a blit - and sends glFinish one-way; the
   present thread, once the batch is done, draws the slot into the chain's
   back buffer the right way up and presents it, and DWM composes it from
   the host's texture. A window whose chain cannot be had reads back as
   before.

   Vulkan done: a swapchain opens its window's chain with one slot in its
   images' own channel order, rows top first (`SlotLayout`), so a present
   copies the image into the slot with vkCmdCopyImage - its bytes unconverted,
   an sRGB image into the slot's linear format - releases the slot to the
   external queue, waits for the copy, and has the chain copy the slot into
   its back buffer and present it (`ChainedImage`, `PresentDevice::copy_into`).
   Presents stay synchronous and paced as before. A swapchain recreated with
   the old one named retires the old one first, which gives its slot back, so
   the new one opens the window's chain again; a retired swapchain's presents
   are consumed and shown nowhere.

   Verified in the guest with `vk_window.exe` and `gl_window.exe`, captured as
   the whole screen (`wgc_probe screen`): the window's region holds its frame
   in all 180 frames of 3 s at 60 Hz, before and after a resize. Captured as a
   window instead, WGC delivers the odd frame black over the title bar too -
   a plain D3D11 flip window does the same - which is the capture's, not the
   screen's.
6. **The renderer and other GPUs.** Staging slots cross to QEMU, and a surface
   opened on another GPU gets a copy path.

## 5. Testing

- **Unit.** `SharedSurfaces` against a mock `SurfaceApi` covers rows, points,
  learning, staging eviction, release, reset, capacity and lock discipline.
  Each `SurfaceApi` is covered by the device tests (`test_d3d12_session_device`,
  `test_d3d_backend_device`) on a real GPU. A key case: a D3D12 surface
  published at point N, read at N by the "engine", while the queue goes on to
  draw N+1, must read N's pixels.
- **Fuzz.** Every new decoded record (`PresentPointRecord`, the engine's
  `(id, point)` source) gets a target.
- **Guest.** The probes under `tools/guest/probes/`: `d3d9_probe`, and the D3D11
  blt and D3D12 flip references used for this note. A D3D9 window, a D3D11 blt
  window and a D3D12 flip window each show their clear colour, and the desktop
  (Settings, Start) and the event log stay clean.

## 6. Decisions for review

1. **Whose device stages and exports.** Keep borrowing DXVK's queue
   (`DxvkVulkanDevice`), or give `SharedSurfaces` a small Vulkan device per GPU
   of its own? The writer's queue stages in either case (§2.5); this is about
   scanout export and cross-GPU copies, which today run on DXVK's device under
   the D3D11 lock. **Recommendation:** a device of its own, because it takes
   scanout off the D3D11 lock.
2. **Blt staging.** The proposal is eager staging for surfaces a blt has read
   before (§2.5), which costs one direct read of a new surface. The alternative
   is to have the writer wait for the reader: no copy, but a guest queue then
   depends on the present engine, and a present that never comes hangs it.
   **Recommendation:** staging.
3. **Scope.** Per device, matching the id space (proposed), or per GPU, as
   Direct3D's table is today.
4. **Keyed mutexes.** Make `IDXGIKeyedMutex` a writer/reader pair of points in
   the same model, or leave it for later.

## 7. Documents found out of date while surveying

- `protocol.md`: the allocation descriptor is 56 bytes, not 48, and the shared
  private data is 96 bytes (`kb/04-memory.md` says 88).
- `protocol.md`: "OpenCL does neither yet" - OpenCL opens shared objects now.
- `protocol.md` 0x02F7: `source_key` is a random snapshot key, not a
  surface's key, until step 2 makes it one.
- `d3d.toml` `CreateResource`: the host keeps a shared resource until
  `SharedReleaseRequest`, not until the last session destroys it.
- `d3d-desktop.md` §3.1: the key is not in the 16-byte resource data.
  §3.3: the scanout commands are `SCANOUT_IMAGE` and `RESOURCE_FLUSH`.
  §3.5: KMD-minted keys for GDI surfaces are not implemented.
