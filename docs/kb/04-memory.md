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
