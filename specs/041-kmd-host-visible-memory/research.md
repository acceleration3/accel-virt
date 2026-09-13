# Research: KMD host-visible memory

Everything here was read out of this repository, the pinned WDK, or the QEMU
tree in `third_party/qemu`; each decision names where it came from.

## 1. What the device already offers, and what it does not

`virtio-gpu-pci` creates the host-visible region itself: a `MemoryRegion`
sized by the `hostmem=` property, registered as **BAR 4** with a virtio
shared-memory capability of **shm id 1**
(`third_party/qemu/qemu-11.1.1/hw/display/virtio-gpu-pci.c:35-46`). The rvk
device reserves that many bytes of anonymous memory at realize and adds it at
offset 0 of the region (`src/device/qemu/virtio-gpu-rvk.c:40-50`), so an
unmapped offset reads as zeros.

A host-visible blob is `RESOURCE_CREATE_BLOB` with `BLOB_MEM_HOST3D`,
`FLAG_USE_MAPPABLE` and **no memory entries**
(`src/device/control/blob_commands.cpp:57-83`), backed by a memfd
(`src/device/memory/host_memory_block.cpp:12-21`). `RESOURCE_MAP_BLOB` places
it at a **guest-chosen, page-aligned offset** inside the region and answers
`OK_MAP_INFO` with `MAP_CACHE_CACHED`
(`blob_commands.cpp:95-107`, `docs/protocol.md:403-435`). The device only
*judges* the offset - inside the region, page-aligned, not overlapping another
mapping (`src/device/memory/blob_table.cpp:51-60`).

**Decision.** The offset on the wire is the guest's to send, but inside the
guest it is **VidMm's to choose**: the segment is reported to VidMm, so VidMm
owns its address space and decides where an allocation sits in it. The driver
learns that offset from the paging operation that makes the allocation resident
and sends it on. What the driver owns is the judgement the device will make of
it - page-aligned, wholly inside the region, overlapping nothing already mapped
- kept first, so a refusal from the device is a bug in this driver. No device
change is needed for placement.

(This corrects the first shape of this plan, which had the driver allocating
offsets itself. It cannot: the CPU address a process is handed is the segment's
`CpuTranslatedAddress` plus the offset **VidMm** placed the allocation at, so a
blob mapped anywhere else would be memory the process never sees.)

**Decision.** The live runner must give the device a region: it never passes
`hostmem=` today (`tools/guest/run_guest_suite.py:176`), so on the real guest
`hostmem_size()` is 0 and `feature_host_visible_memory` is clear
(`src/device/probe/host_feasibility.cpp:29-34`). The runner passes
`hostmem=256M` from this feature on, and `--hostmem` chooses another size.

## 2. Which segment shape to report

The WDK offers two ways to say "the CPU can address this segment"
(`third_party/windows-kit/include/wdk-shared/d3dkmddi.h:2720-2749`):

- `Flags.CpuVisible` with `CpuTranslatedAddress` - the CPU physical base of the
  whole segment;
- `Flags.SupportsCpuHostAperture` with `DXGK_CPUHOSTAPERTURE` - a window the
  driver maps pages into on demand, through `DxgkDdiMapCpuHostAperture` and
  `DxgkDdiUnmapCpuHostAperture` (`km/dispmprt.h:2877`).

**Decision: `CpuTranslatedAddress`.** The whole BAR is addressable at once and
its guest physical base is known from the PCI capability walk, so the aperture
variant would buy nothing and cost two more DDIs and a page-level remapping
path. `SupportsCpuHostAperture` stays clear, which is what makes the union mean
`CpuTranslatedAddress` (:2730-2737).

**Decision: segment identifiers shift.** `ApertureSegment` is segment 1 and
VidMm appends its own system-memory segment after the last one the driver
reports (`src/core/memory/aperture_segment.hpp:25-27`). Reporting a second
segment moves that implicit one from 2 to 3, so the number a non-primary
allocation is sent to cannot stay a constant - it becomes the count the segment
list reports plus one.

**Decision: the segment is not an aperture.** It has storage: the blobs mapped
into it. `Flags.Aperture` stays clear, so VidMm never asks for
`MAP_APERTURE_SEGMENT` on it and never expects guest pages behind it.

## 3. How an allocation's blob follows VidMm's placement

VidMm owns the segment's address space: it decides at which offset an
allocation sits. Our storage only exists once a blob is mapped there, and the
only call that tells the driver an allocation is being made resident in a
segment is a paging operation naming it (`DXGK_OPERATION_TRANSFER` in, and out
again on eviction; `src/core/memory/paging_request.cpp:31-49` is where the
existing ones are decoded).

**What dxgkrnl does, read rather than assumed.**
`VIDMM_GLOBAL::MapInCpuVisibleSegment` (dxgmms2.sys @14009ea98) builds an MDL
over the segment's CPU range itself - `VidMmiBuildMdlForContiguousMmIo` for a
segment whose storage is MMIO, which is what a BAR is - and locks it for the
process. **VidMm maps; the driver is never called to map anything for the CPU.**
The driver's whole job is to have storage at the offset VidMm chose, and the
`CpuTranslatedAddress` it reported is what makes that offset an address.

**Decision.** One blob per host-visible allocation, created and mapped into the
region when the first paging operation names its offset in this segment, and
released when the allocation is destroyed. The blob outlives an unmap - the
device keeps its contents (`docs/protocol.md:416-420`) - so a later re-map at
another offset does not lose what the process wrote.

**Decision: nothing is evicted out of this segment.** A host-visible allocation
is reported with the host-visible segment as its only supported segment and an
empty `EvictionSegmentSet`, so VidMm has one place to put it and nowhere to move
it to. That is what makes "no copy" true, and it is also what keeps the driver
out of the business of moving bytes between segments, which for a paravirtual
device would mean copying the whole allocation through the BAR. The region
filling up is then a creation failure, which VidMm turns into a budget failure
(FR-006), not a bugcheck.

**Open, and the live guest is what answers it.** Which paging operation carries
that first offset - `DXGK_OPERATION_TRANSFER` into the segment, or
`DXGK_OPERATION_FILL` - is not settled by reading: both name a destination
segment and address, and which one Windows issues for a never-yet-resident
CPU-visible allocation depends on the allocation's backing store. The driver
therefore handles **both**, and the L5 run says which one arrived.

**Decision.** A process's writes go straight through the BAR: no command is
sent per access, which is what SC-002 measures.

## 4. What "no host-visible memory" must mean

`feature_host_visible_memory` says the device has a region at all;
`feature_gpu_exported_memory` says memory allocated on the host GPU can be
placed in it and read back correctly (`docs/kb/04-memory.md:160-211`: passed on
Intel Iris Xe and on NVIDIA through dma-buf). The driver reads the capset at
StartDevice already (feature 040's `CapsetExchange`), so the answer is in hand
before a segment is reported.

**Decision.** The CPU-visible segment is reported only when the capset sets
`feature_host_visible_memory` **and** the PCI capability walk found shared
memory region 1. Otherwise the adapter reports the one aperture segment it
reports today, a host-visible allocation is created guest-backed, and the
allocation says so - which is what FR-007 asks to be observable.

## 5. Rings and fence pages

An rvk context's ring, fence page and reply slot are kernel allocations today
(`src/core/rvk/context_memory.cpp:22-37`), sent to the device as guest blobs
and never seen by a process. Feature 043 is what gives a process a ring of its
own; what 041 owes it is that those pages can *be* allocations - pinned for the
context's lifetime, mappable by the process that owns the device, and refused
to anyone else.

**Decision.** `ContextMemory` keeps its kernel path (the driver's own contexts
need no process), and the allocation kinds for a ring and a fence page are
added to the descriptor now, with the residency rule and the ownership check
tested at L1 and L2. Mapping one into a real process is 043's, and this
feature does not pretend to test it.

## 6. Alternatives considered

- **A fake local video memory segment.** Refused, and the knowledge base says
  why (`docs/kb/04-memory.md:20-27`): VidMm places allocations there and then
  asks for a transfer into storage that does not exist.
- **Mapping guest pages into the host GPU instead** (`feature_guest_memory_import`,
  `VK_EXT_external_memory_host`). That is the other half of 039's finding and
  belongs to the executor (042): it makes the *host* read guest memory, which is
  not what a process mapping host memory needs.
- **`SupportsCpuHostAperture`.** Refused above.
- **Letting the device choose the offset.** It cannot: `RESOURCE_MAP_BLOB`
  carries the offset from the guest, and changing that would break the protocol
  document and every qtest that pins it.
