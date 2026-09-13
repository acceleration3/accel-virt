# Data model: KMD host-visible memory

Four classes, and the wire records they exchange. Every state listed is private;
what is written here is what the class owns, not what it exposes.

## `HostVisibleSegment` (object class)

One per adapter, beside `ApertureSegment`.

| State | Meaning |
|---|---|
| `base_` | the region's guest physical base, from the PCI shared-memory capability with id 1 |
| `size_` | the region's size in bytes, page-aligned down |
| `offered_` | whether the device offers it: the capset's `feature_host_visible_memory`, and a region that was found |

**Transitions.** Constructed empty and not offered. `initialize(region, capset)`
takes the region the PCI walk found and the capset the adapter read at
StartDevice; it is offered only when the capset sets the bit and the region has
a non-zero size. `forget()` at StopDevice.

**What it fills** in a `DxgkSegmentDescriptor4Record`: `Flags` =
`CpuVisible | CacheCoherent` (`0x14`) - not `Aperture`, because the segment has
storage of its own - `BaseAddress` = 0 (its own linear space), `Size` and
`CommitLimit` = `size_`, and `CpuTranslatedAddress` = `base_`.
`SupportsCpuHostAperture` stays clear, which is what makes the union member
mean `CpuTranslatedAddress`.

**Invariant**: not offered unless `size_` is a non-zero page multiple and
`base_` is page-aligned.

## `MemorySegments` (system class)

Owns the aperture segment and the host-visible one, and is the only thing that
knows how many the adapter reports.

| State | Meaning |
|---|---|
| `aperture_` | the aperture segment over guest system memory (segment 1, unchanged) |
| `host_visible_` | the CPU-visible segment (segment 2, when offered) |

**What it answers.** `count()` - 1 or 2. `report(query)` for
`DXGKQAITYPE_QUERYSEGMENT4`: the count on the first pass, then that many
descriptors at the caller's stride, the paging buffer named on segment 1 as
today. `system_memory_segment_id()` - `count() + 1`, because VidMm appends its
own system memory segment after the last one reported; today's constant 2 was
only right while there was one.

**Invariant**: the host-visible segment is reported if and only if it is
offered, and segment identifiers are 1..`count()` with no gap.

## `HostmemPlacement` - not built

It was to record which allocation held which part of the region. With the
region mapped as one blob and VidMm owning the segment's address space, nothing
in this feature asks that question, and the constitution's rule against
speculative structure says not to answer it yet. Feature 042's executor is the
caller that will need it.

## `HostVisibleBlob` (object class)

One per adapter: the whole hostmem region as a single blob, created and mapped
at offset 0 when the segment is offered, so every offset VidMm can place an
allocation at already has host memory behind it. (VidMm maps a process's view
itself and tells the driver nothing, so there is no moment at which a
per-allocation blob could be created in time.)

| State | Meaning |
|---|---|
| `resource_` | the device resource id the blob was created with |
| `offset_` | where it is mapped in the region, when it is |
| `pages_` | its length |
| `mapped_` | whether the device holds a mapping for it now |

**Transitions.**

```
created ──create()──▶ unmapped ──map(offset)──▶ mapped
   ▲                     │  ▲                      │
   └────destroy()────────┘  └──────unmap()─────────┘
```

- `create(size)` sends `RESOURCE_CREATE_BLOB` (`BLOB_MEM_HOST3D`,
  `FLAG_USE_MAPPABLE`, no memory entries) and keeps the resource id.
- `map(offset)` sends `RESOURCE_MAP_BLOB` and judges the answer: `OK_MAP_INFO`
  with `MAP_CACHE_CACHED`, anything else is a refusal and nothing is recorded as
  mapped.
- `unmap()` on eviction; the device keeps the contents, so a later `map` at
  another offset brings them back.
- `destroy()` sends `RESOURCE_UNREF`.

**Invariant**: `offset_` and `pages_` mean something only while `mapped_`; a
blob with no resource id is neither mapped nor destroyable.

## Records on the wire

| Record | Direction | What this feature adds |
|---|---|---|
| `RESOURCE_CREATE_BLOB` | guest → device | the host-visible shape: `blob_mem = HOST3D`, `flags = USE_MAPPABLE`, `nr_entries = 0`, `blob_id = 0` |
| `RESOURCE_MAP_BLOB` | guest → device | resource id and the offset VidMm placed the allocation at |
| `OK_MAP_INFO` | device → guest | `map_info`, which must be `MAP_CACHE_CACHED` |
| `RESOURCE_UNMAP_BLOB` | guest → device | on eviction and on destroy |
| `AllocationDescriptor` | process → guest | unchanged in shape; `flag_cpu_visible` now decides a segment rather than a flag echoed back |

## What an allocation reports

`Allocation::report_to` chooses the segment:

| Allocation | Preferred segment | Why |
|---|---|---|
| `shared_primary` | the aperture (1) | the display reads it through guest pages, and it must be `AccessedPhysically` |
| any kind with `flag_cpu_visible`, region offered | the host-visible segment (2) | this is the memory a process is asking to map |
| any kind with `flag_cpu_visible`, no region | VidMm's system memory | the fallback, and the allocation says so (FR-007) |
| everything else | VidMm's system memory | unchanged |
