# 04 — Memory: segments, residency, paging, GPU virtual addressing

## Segments

The KMD tells VidMm what memory pools exist by answering
`DXGKQAITYPE_QUERYSEGMENTCOUNT` then `DXGKQAITYPE_QUERYSEGMENT5`
(`DXGK_SEGMENTDESCRIPTOR5`; revisions 1–5 exist, use the highest your DDI
level allows). Per segment you declare:

- `BaseAddress` / `Size` — where it lives in **segment address space**, which
  is a per-segment linear space, not a GPU virtual address.
- `CpuTranslatedAddress` + `Flags.CpuVisible` — if the CPU can map it (a BAR).
- `Flags.Aperture` — the segment is an aperture: it has no storage of its own,
  and VidMm asks you to map system pages into it via
  `DXGK_OPERATION_MAP_APERTURE_SEGMENT`.
- `Flags.PopulatedFromSystemMemory`, `Flags.UseBanking`, `Flags.CacheCoherent`.

The classic layout is one local-video-memory segment plus one aperture segment
for system memory the GPU reads over the bus.

**For a paravirtual driver:** the natural mapping is an aperture segment
backed by guest system memory the device can read, plus a CPU-visible segment
whose `CpuTranslatedAddress` is a virtio-gpu host-memory BAR (`hostmem`), into
which the device maps real host GPU memory. See `11-implementation-plan.md`. Do not
invent a fake local VRAM segment with no storage behind it — VidMm will place
allocations there and then ask you to `DXGK_OPERATION_TRANSFER` bytes into a
place that does not exist.

### The CPU-visible segment, as feature 041 built it

**Superseded by the host window (`docs/host-window.md`).** The driver no longer
reports the region as a segment or maps it as one blob: on WDDM 3.x VidMm
refuses a CPU-visible allocation whose segment set has no aperture segment, and
with the aperture in the set it hands out the aperture's staging pages at Lock
rather than the region. The driver now places one host blob per chunk itself
and maps it into the process that asked. What follows is kept for what it
learnt about VidMm.

Three things were settled by reading `dxgmms2.sys`, and each of them changed the
design:

1. **VidMm maps a process's view itself.**
   `VIDMM_GLOBAL::MapInCpuVisibleSegment` (@14009ea98) builds an MDL over the
   segment's CPU range with `VidMmiBuildMdlForContiguousMmIo` — the MMIO case a
   BAR is — and locks it. The driver is never called to map anything for the
   CPU. Its whole job is to have storage at the offset VidMm chose, and the
   `CpuTranslatedAddress` it reported is what turns that offset into an address.
2. **A GPU-MMU driver is told about placement only through page tables.** VidMm
   issues the physical `FILL` and `TRANSFER` operations to WDDMv1 drivers only;
   for a WDDMv2 driver the sole paging operation naming a segment is
   `UPDATE_PAGE_TABLE`, whose entries carry a segment id and a page frame — and
   that arrives only for an allocation the GPU is given an address for. An
   allocation a process merely locks produces no driver call at all.
3. **Therefore the region is one blob.** The driver creates a single
   host-visible blob the size of the whole region and maps it at offset 0 when
   the adapter starts, so every offset VidMm can choose already has host memory
   behind it. A blob per allocation cannot work: there is no call in which it
   could be created in time, and the case that matters — a process mapping host
   memory — would get a BAR window with nothing behind it, reading zeros and
   losing what it wrote.

What the segment says: `Flags` = `CpuVisible | CacheCoherent` and never
`Aperture` (an aperture has no storage of its own and VidMm would ask for guest
pages to be mapped into it), `BaseAddress` 0, `Size` and `CommitLimit` the
region's size, `CpuTranslatedAddress` the region's guest physical base.
`SupportsCpuHostAperture` stays clear, which is what makes the union in
`DXGK_SEGMENTDESCRIPTOR4` mean `CpuTranslatedAddress` rather than a
`DXGK_CPUHOSTAPERTURE`.

An allocation that asks to be mapped names that segment as its only supported
one with an empty `EvictionSegmentSet`: VidMm then has one place to put it and
nowhere to move it to, which is what makes a process's view the host's memory
rather than a copy of it — and keeps the driver out of transferring whole
allocations through a BAR.

Reporting a second segment also moves VidMm's own implicit system-memory
segment from 2 to 3, so the number an allocation with no segment of its own is
sent to follows the count of segments reported and cannot be a constant.

### What a primary's placement really turns on

Reported and tested in feature 038 on a real Windows 11 guest: **an allocation
the display scans out must be marked
`DXGK_ALLOCATIONINFOFLAGS.AccessedPhysically` (0x8000) in
`DxgkDdiCreateAllocation`.** Without it VidMm leaves the primary in its own
implicit system-memory segment whatever `PreferredSegment` and
`SupportedRead/WriteSegmentSet` say, `DXGK_OPERATION_MAP_APERTURE_SEGMENT` is
never called for it, and `DxgkDdiSetVidPnSourceAddress` answers
`PrimarySegment` 0, `PrimaryAddress` 0 - an address with no pages behind it.
With it, VidMm pins the allocation in the segment you prefer, maps it into the
aperture with an MDL of its guest pages, and the source address becomes
`BaseAddress + offset`. The display has no GPU MMU to read through, which is
what the flag is telling VidMm.

The corollary for a paravirtual driver: **the pages are the only thing worth
recording.** An aperture address means nothing to a device that reads guest
physical memory, so `DXGK_OPERATION_MAP_APERTURE_SEGMENT` is where the driver
learns where a frame is - one MDL per mapping, read with `MmGetMdlPfnArray`,
kept per allocation and turned back into memory entries when the host is told
to read it.

## Residency and the paging queue

WDDM 2.0 made this asynchronous and it has not changed since. The flow:

```
UMD/runtime           dxgkrnl / VidMm                    KMD
  MakeResident   ──►  pick a segment, maybe evict
                      something else
                      ──────────────────────────────►  DxgkDdiBuildPagingBuffer
                                                        (one call per operation)
                      ◄──────────────────────────────  filled DMA buffer
                      submit on the paging node ────►  DxgkDdiSubmitCommand
                                                        (paging context)
                      ◄──────────── fence ────────────  DxgkCbNotifyInterrupt
  monitored fence
  signalled
```

`DxgkDdiBuildPagingBuffer` is called with a `DXGK_BUILDPAGINGBUFFER_OPERATION`
discriminated union. The complete operation set in WDDM 3.2:

| Operation | Meaning |
|---|---|
| `TRANSFER` | Copy an allocation between segments (or to/from system memory). |
| `FILL` | Fill a range with a pattern. |
| `DISCARD_CONTENT` | Contents are now undefined; skip the copy. |
| `READ_PHYSICAL` / `WRITE_PHYSICAL` | Direct physical access. |
| `MAP_APERTURE_SEGMENT` / `MAP_APERTURE_SEGMENT2` | Program aperture PTEs for a page list. |
| `UNMAP_APERTURE_SEGMENT` | Tear those down. |
| `SPECIAL_LOCK_TRANSFER` | Lock-based transfer for CPU access. |
| `UPDATE_PAGE_TABLE` | Write GPU page-table entries. |
| `COPY_PAGE_TABLE_ENTRIES` | Bulk PTE copy. |
| `FLUSH_TLB` | Invalidate the GPU TLB. |
| `MAP_MMU` / `UNMAP_MMU` | Map/unmap through a named MMU (multi-MMU GPUs). |
| `UPDATE_CONTEXT_ALLOCATION` | Update a context-private allocation. |
| `INIT_CONTEXT_RESOURCE` | Initialize context resources. |
| `NOTIFY_ALLOC` | Informational: allocation state changed. |
| `NOTIFY_RESIDENCY` / `NOTIFY_RESIDENCY2` | Informational: residency changed. |
| `NOTIFY_FENCE_RESIDENCY` | Residency of a fence's storage changed. |
| `SIGNAL_MONITORED_FENCE` | Signal a monitored fence from the paging engine. |
| `VIRTUAL_TRANSFER` / `VIRTUAL_FILL` | The GPU-VA-addressed forms of TRANSFER/FILL. |

You are given a DMA buffer to write into and you return how many bytes you
used. Nothing executes at `BuildPagingBuffer` time — the buffer is submitted
afterwards like any other.

**For a paravirtual driver:** most of these become either a no-op or a single
serialized host message. `TRANSFER` between an aperture segment and system
memory, when the host already has coherent access to guest RAM, is genuinely
nothing — return `STATUS_SUCCESS` with zero bytes written. Resist the urge to
implement real copies for segments that are the same memory viewed twice.

## GPU virtual addressing

Two addressing models exist and you choose one at `QueryAdapterInfo` time.

### Legacy: physical addressing + patching

Report no GPU MMU support. VidMm gives you, per submission, an **allocation
list** and a **patch location list**. `DxgkDdiPatch` is called after placement
with the final segment addresses, and you write them into the DMA buffer at the
recorded offsets. `DxgkDdiSubmitCommand` then takes a physical DMA address.

This model is simpler but it is legacy, it forces a patch pass over every
buffer, and several modern features (hardware queues, user-mode submission,
native fences) are unavailable.

### Modern: GPU virtual addressing

Report `DXGK_GPUMMUCAPS` via `DXGKQAITYPE_GPUMMUCAPS`, and page-table geometry
via `DXGKQAITYPE_PAGETABLELEVELDESC`. Then:

- Each device gets a GPU virtual address space. `D3DKMTReserveGpuVirtualAddress`
  / `MapGpuVirtualAddress` / `FreeGpuVirtualAddress` manage it from user mode.
- VidMm owns the page tables. It calls `BuildPagingBuffer` with
  `UPDATE_PAGE_TABLE` / `COPY_PAGE_TABLE_ENTRIES` / `FLUSH_TLB` and you emit
  whatever your hardware needs.
- `DxgkDdiPatch` is **not** called. The UMD writes GPU VAs directly into the
  command stream at record time, because a VA is stable across residency
  changes.
- `DxgkDdiSubmitCommandVirtual` replaces `SubmitCommand` and carries
  `D3DGPU_VIRTUAL_ADDRESS DmaBufferVirtualAddress` instead of a physical
  address.

`DXGK_GPUMMUCAPS` also declares `VirtualAddressBitCount`, page sizes, and
whether you support `CacheCoherent` mappings and per-level PTE formats.

**For a paravirtual driver this is the right choice and it is easier than it
looks.** You are not implementing an MMU; you are implementing a *bookkeeping*
MMU. Options, in increasing order of fidelity:

1. **Identity/opaque VA.** Define the GPU VA of an allocation to be a token
   your host understands (e.g. a 48-bit host object id plus offset). Page table
   updates become "record that VA range R now refers to host object O at
   offset X". `FLUSH_TLB` is a no-op. This is the least work and it composes
   perfectly with a remoting protocol, because the host resolves the token to
   a real `VkBuffer`/`VkDeviceMemory` rather than to an address.
2. **Real shadow page tables in guest memory** that the host walks. Needed only
   if the host must dereference guest-authored addresses without a per-access
   translation call.
3. **Real hardware page tables.** Not applicable.

Option 1 is what you want. The constraint it imposes is that the *UMD* must be
the only thing that writes GPU VAs into command buffers — which is true by
construction, because the DMA buffer format is yours.

## Allocation lifetime traps

- `DxgkDdiCreateAllocation` can be called with `Flags.Resource` for a
  multi-allocation resource; the resource handle is what gets shared, not the
  individual allocations.
- `DxgkDdiOpenAllocation` is called in *each* process that opens a shared
  resource, and the per-process driver data is separate from the per-allocation
  data. A remoting design must key host-side objects off the global allocation
  identity, not the per-open handle.
- `DxgkDdiDescribeAllocation` must work for any allocation VidMm might move,
  including standard allocations you did not author.

**What feature 053 built for the second trap.** The per-open handle really is
useless as an identity, so the identity is minted where nothing can forge it
and travels on the one channel that crosses a process boundary: the creating
process's *allocation* private driver data, which dxgkrnl hands the opening
process and reads none of. Private data of 88 bytes is a descriptor followed by
a `SharedObjectRecord`; `DxgkDdiCreateAllocation` mints a 64-bit id and the
adapter's own number into it and writes both back into the creator's own bytes.
`SharedObjectTable` is one per adapter, and at open it believes nothing it did
not write - an id it never minted, an id another adapter minted, a record that
does not decode, a kind or a size that disagrees. `HostSharedObjects` is the
other half, on the device: one host descriptor per id for the whole device,
refcounted by the contexts holding it, so the object outlives the context that
published it and the descriptor is closed exactly once.

## What the host GPUs here can actually do (feature 039)

Measured, not assumed: `tools/host/probe_host_memory.py --qemu build-qemu/qemu`
starts one device per host GPU and reads back what its probes found at realize.
Each answer is a capset bit the guest gets only where the probe passed.

| Host GPU | Driver | Exported memory in the hostmem BAR | Guest pages imported | Measured |
|---|---|---|---|---|
| Intel Iris Xe (ADL GT2), `8086:46a6` | Mesa ANV 26.2.2 | passed | passed | 1 MiB each way |
| NVIDIA RTX 3070 Ti Laptop, `10de:24e0` | NVIDIA proprietary 580 | passed, **dma-buf only** | passed | 1 MiB each way |
| AMD | — | not measured: no AMD GPU in this machine | not measured | — |
| llvmpipe / lavapipe | — | not measured: not installed here | not measured | — |

**The handle type is the whole answer on NVIDIA.** `vkAllocateMemory` with
`VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT` succeeds and
`vkGetMemoryFdKHR` gives a descriptor, and then `mmap` of that descriptor fails
with `EINVAL`. The same allocation exported as
`VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT` maps, is placed over the
hostmem reservation with `MAP_FIXED`, and reads back what the device wrote.
Mesa takes either. A driver that refuses both is not a broken host - it is a
device whose `feature_gpu_exported_memory` bit stays clear and whose guest
copies instead.

**What the probes are, precisely.** Each runs once, inside realize, on the GPU
the instance is bound to, over one megabyte it releases before realize returns:

- *Exported memory*: allocate on the host GPU, place at offset 0 of the hostmem
  region, write a pattern through the device's own mapping, and read the region
  back - what the guest would read there, not what the block holds. A mapping
  that exists and shows something else is worse than a refusal, and is recorded
  as `malformed`.
- *Guest import*: map a window of guest RAM, refuse anything that is not a
  direct mapping (a bounce buffer is a copy the guest cannot see), import it
  with `VK_EXT_external_memory_host`, and compare what the GPU reads at the
  window's first and last sample with what was written. The window is the
  guest's memory, so the probe keeps a copy and puts it back; timing that copy
  is what a refused import reports as the fallback's throughput.

**Two traps in the device's own realize**, both found live: a virtio device has
no DMA address space until the bus plugs it in, which is after realize returns,
so anything mapping guest RAM that early must use the machine's address space
(`address_space_memory`) or segfault QEMU; and guest RAM must be
`memory-backend-memfd,share=on` for the host GPU to import it at all.
