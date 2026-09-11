# The rvk wire protocol

Normative layout of every structure the guest driver and the QEMU device
exchange outside the stock virtio-gpu protocol. `src/proto` implements it; both
sides compile that one implementation, so the two can never disagree about a
byte. The encoded forms below are the contract - no structure is ever cast from
raw bytes.

## Conventions

- All integers are little-endian. Offsets and sizes are in bytes.
- Every structure is fixed-size and starts with a 32-bit magic made of four
  ASCII characters, read as a little-endian integer.
- Reserved fields MUST be zero when written and are rejected when non-zero:
  a later version that needs the space bumps the minor version.
- Every structure is read from memory the other side can change at any time.
  A decoder copies the bytes once and validates the copy; nothing re-reads the
  shared memory after validation.
- Monotonic counters are 64-bit and never wrap in practice (2^64 bytes or
  fences); a counter that moves backwards is a protocol violation.

Decoder outcomes: `ok`; `malformed` for bytes no valid peer can produce (short
buffer, bad magic, non-zero reserved, out-of-range field); `not_supported` for
a well-formed request this side cannot honour (major version, sub-API);
`invalid_state` for values inconsistent with what was already observed
(regressions, completions of unsubmitted fences).

## Version

`major.minor`, both `u16`. This document is **1.0**.

Two peers are compatible when their majors are equal. The negotiated version is
`major.min(local minor, peer minor)`. A structure's layout depends only on the
major; minors add meaning to feature bits and reserved-free fields only.

## Capset (device -> guest)

virtio-gpu capset id **0x72**, capset version 1, returned by
`VIRTIO_GPU_CMD_GET_CAPSET`. One per device instance, i.e. one per host GPU
(`docs/kb/11-implementation-plan.md`, "One adapter per host GPU"). 128 bytes.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `RVK1` = `0x314B5652` |
| 4 | u16 | major | compatible with the guest |
| 6 | u16 | minor | |
| 8 | u32 | sub_apis | bit 0 Vulkan, 1 OpenGL, 2 OpenCL; at least one; no other bit |
| 12 | u32 | max_contexts | >= 1 |
| 16 | u8 | min_ring_order | 12 <= min <= max <= 24 (ring data 4 KiB..16 MiB) |
| 17 | u8 | max_ring_order | |
| 18 | u16 | reserved | 0 |
| 20 | u32 | reserved | 0 |
| 24 | u64 | features | bit 0 `host_visible_memory`; unknown bits are ignored by the reader |
| 32 | u32 | vendor_id | `VkPhysicalDeviceProperties::vendorID` |
| 36 | u32 | device_id | `::deviceID` |
| 40 | u32 | device_type | `VkPhysicalDeviceType`: 0 other, 1 integrated, 2 discrete, 3 virtual, 4 cpu |
| 44 | u32 | reserved | 0 |
| 48 | u8[16] | device_uuid | `VkPhysicalDeviceIDProperties::deviceUUID` |
| 64 | u64 | local_memory | bytes in device-local heaps |
| 72 | u64 | shared_memory | bytes in host-visible, non-device-local heaps |
| 80 | u8[48] | name | UTF-8, NUL-terminated inside the field, every byte after the first NUL zero |

Features are the only field where unknown values are tolerated: a newer device
may advertise a capability an older guest does not use.

## Context setup (guest -> device)

Sent in an rvk control command after `VIRTIO_GPU_CMD_CTX_CREATE` with context
init capset id 0x72. It binds the context to its ring and fence page. 32 bytes.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `RVKC` = `0x434B5652` |
| 4 | u16 | major | the negotiated version |
| 6 | u16 | minor | |
| 8 | u32 | sub_api | exactly one bit, one of the capset's `sub_apis` |
| 12 | u32 | ring_resource | virtio-gpu resource id, non-zero |
| 16 | u8 | ring_order | within the capset's [min, max] |
| 17 | u8[3] | reserved | 0 |
| 20 | u32 | fence_resource | non-zero, different from `ring_resource` |
| 24 | u64 | reserved | 0 |

The checks that need the capset (major, sub_api membership, ring order range)
are a separate validation against a decoded capset; the decoder checks what the
bytes alone can prove.

## Ring header (shared, both write)

Offset 0 of the ring resource; the ring's data area follows at offset 32 and
holds `2^order` bytes. 32 bytes.

| Offset | Type | Field | Writer | Rule |
|--:|---|---|---|---|
| 0 | u32 | magic | guest, once | `RVKR` = `0x524B5652` |
| 4 | u8 | order | guest, once | 12..24 |
| 5 | u8[3] | reserved | | 0 |
| 8 | u64 | tail | guest | total bytes produced |
| 16 | u64 | head | device | total bytes consumed |
| 24 | u64 | reserved | | 0 |

A byte's position in the data area is `counter & (2^order - 1)`. At every
observation `head <= tail <= head + 2^order`, and neither counter is smaller
than the last value observed. Each side trusts only its own counter and
validates the other's against its last observation.

## Submission descriptor (guest -> device)

One batch of ring bytes and the fence that completes it; carried as the DMA
buffer of a submission. 32 bytes.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `RVKS` = `0x534B5652` |
| 4 | u32 | flags | 0 |
| 8 | u64 | fence_id | >= 1 |
| 16 | u64 | begin | ring tail counter before the batch |
| 24 | u64 | end | ring tail counter after it; `begin < end` |

Against the ring: `end - begin <= 2^order`.

## Allocation descriptor (user mode -> guest driver)

The private driver data of one allocation. `DxgkDdiGetStandardAllocationDriverData`
writes it for the runtime's standard allocations; a user-mode driver writes it
for its own; `DxgkDdiCreateAllocation` decodes it. It never reaches the device.
48 bytes.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `RVKA` = `0x414B5652` |
| 4 | u16 | kind | 1 shared primary, 2 shadow, 3 staging, 4 GDI surface (the `D3DKMDT_STANDARDALLOCATION_TYPE` values), 16 client |
| 6 | u16 | flags | bit 0 CPU visible; others 0 |
| 8 | u32 | width | pixels |
| 12 | u32 | height | pixels |
| 16 | u32 | format | `D3DDDIFORMAT` |
| 20 | u32 | pitch | bytes per row; `pitch >= width` |
| 24 | u64 | size | bytes, >= 1; `pitch * height <= size` |
| 32 | u32 | refresh_numerator | shared primary only, else 0 |
| 36 | u32 | refresh_denominator | shared primary only, else 0 |
| 40 | u32 | source | VidPn source; shared primary only, else 0 |
| 44 | u32 | reserved | 0 |

Every kind but client is a surface: width, height and pitch are non-zero.

## Present command (guest driver -> device, in a DMA buffer)

What `DxgkDdiPresent` writes at the start of the DMA buffer dxgkrnl gives it,
followed by its sub-rectangles; `DxgkDdiPatch` fills the addresses before the
buffer is submitted. 72 bytes, then 16 per sub-rectangle. A rectangle is
`left, top, right, bottom` as i32, right and bottom exclusive, `left <= right`,
`top <= bottom`.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `RVKP` = `0x504B5652` |
| 4 | u32 | operation | 1 blt (source to destination), 2 colour fill (destination), 3 flip (source becomes the scanout) |
| 8 | u32 | color | X8R8G8B8, colour fill only, else 0 |
| 12 | u32 | sub_rect_count | rectangles following the header |
| 16 | rect | source | in the source allocation, as dxgkrnl gave it; read for blt |
| 32 | rect | destination | in the destination allocation |
| 48 | u64 | source_address | allocation 1; blt and flip |
| 56 | u64 | destination_address | allocation 2; blt and colour fill |
| 64 | u32 | flip_interval | `D3DDDI_FLIPINTERVAL_TYPE`, flip only |
| 68 | u32 | address_space | 0: the addresses are physical, patched by `DxgkDdiPatch`; 1: they are GPU virtual addresses in the submitting context's address space, written by `DxgkDdiPresent` |

A context with GPU virtual addressing gets no patch list: dxgkrnl passes each
allocation's GPU virtual address instead (`DXGK_PRESENTALLOCATIONINFO`), and
`DxgkDdiPatch` is never called for it.

When a DMA buffer cannot hold every sub-rectangle, the command carries as many
as fit and dxgkrnl calls `DxgkDdiPresent` again with a fresh buffer for the
rest; each buffer holds one complete command.

## GPU page table entry (guest driver -> device, in guest memory)

The GPU MMU is four levels of 512 entries each over 48-bit GPU virtual
addresses, 4096-byte pages: bits 47..39, 38..30, 29..21 and 20..12 index the
levels from the root. VidMm allocates each page table and hands the driver its
entries; the driver writes them through VidMm's CPU mapping
(`DXGK_OPERATION_UPDATE_PAGE_TABLE`) and the device walks them. A table is
4096 bytes of little-endian u64 entries:

| Bits | Field | Rule |
|--:|---|---|
| 0 | valid | 0: the whole entry is 0 and the address faults |
| 1 | read_only | writes through the entry fault |
| 2 | no_execute | |
| 3 | cache_coherent | |
| 4..8 | segment | the VidMm segment the frame is in; the implicit system memory segment is 2 |
| 9..11 | reserved | 0 |
| 12..63 | page_frame | the guest physical page (system memory segment) or the page within the segment |

The root table is `GetRootPageTableSize` bytes for the entry count
`SetRootPageTable` names, at the segment offset it names.

## Paging commands (guest driver -> device, in a paging buffer)

What `DxgkDdiBuildPagingBuffer` writes for the operations the device carries
out; VidMm submits the buffer on the paging node. One command per call, each
starting with an 8-byte header, all fields little-endian, reserved fields 0.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `RVKM` = `0x4D4B5652` |
| 4 | u32 | operation | 1 flush TLB, 2 fill, 3 transfer, 4 copy page table entries |

**Flush TLB** (40 bytes): forget cached translations for one address space.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 8 | u64 | root_segment_offset | where the root page table is |
| 16 | u32 | root_segment_id | its segment |
| 20 | u32 | reserved | |
| 24 | u64 | start_address | first GPU virtual address |
| 32 | u64 | end_address | one past the last |

**Fill** (32 bytes): write a u32 pattern repeatedly across a range.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 8 | u64 | destination_address | GPU virtual, in the paging process's address space |
| 16 | u64 | size | bytes |
| 24 | u32 | pattern | |
| 28 | u32 | reserved | |

**Transfer** (56 bytes): copy a range between two address spaces.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 8 | u64 | source_address | GPU virtual |
| 16 | u64 | destination_address | GPU virtual |
| 24 | u64 | size | bytes |
| 32 | u64 | source_page_table | root the source address is walked from |
| 40 | u64 | destination_page_table | root the destination address is walked from |
| 48 | u32 | direction | `DXGK_MEMORY_TRANSFER_DIRECTION`: 0 local to system, 1 system to local, 2 local to local |
| 52 | u32 | flags | bit 0 source uses 64 KB pages, bit 1 destination does |

**Copy page table entries** (16 bytes, then 32 per range): copy entries between
page tables, each table named by its GPU virtual address.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 8 | u32 | range_count | ranges following, at least 1 |
| 12 | u32 | reserved | |

Each range:

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u64 | source_page_table | |
| 8 | u64 | destination_page_table | |
| 16 | u32 | source_start | first entry index |
| 20 | u32 | destination_start | first entry index |
| 24 | u32 | count | `start + count <= 512` for both tables |
| 28 | u32 | reserved | |

When a paging buffer cannot hold every range, the command carries as many as
fit and VidMm calls again with a fresh buffer for the rest.

## The driver's own virtio-gpu resources

Besides rvk commands, the driver sends standard virtio-gpu requests (virtio 1.2
§5.7.6) for resources it owns. Resource ids 1 to 15 are reserved for them;
nothing else may create one.

| Id | Resource | Requests |
|--:|---|---|
| 1 | the hardware cursor: 64x64 `B8G8R8A8_UNORM`, one contiguous backing | `RESOURCE_CREATE_2D`, `RESOURCE_ATTACH_BACKING` once per device start; `TRANSFER_TO_HOST_2D` per shape (control queue); `UPDATE_CURSOR`, `MOVE_CURSOR` (cursor queue) |

Control requests travel with a 24-byte response the device answers with
`VIRTIO_GPU_RESP_OK_NODATA`; anything else is logged. Cursor requests carry no
response. The driver does not wait for either.

## Blob resources (guest -> device)

The device answers the virtio-gpu blob requests (virtio 1.2 §5.7.6.1) itself,
with `VIRTIO_GPU_F_RESOURCE_BLOB` not offered and the base class's `blob`
property refused at realize. Two kinds of blob exist; every other combination is
`VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER`.

| `blob_mem` | Flags | Entries | `blob_id` | Backing |
|---|---|---|---|---|
| `GUEST` (1) | any | at least one, covering `size` | 0 | the guest pages, mapped by the device once at creation and held until unreferenced |
| `HOST3D` (2) | `MAPPABLE` | none | 0 | a zeroed, device-allocated page multiple; the guest reaches it through the host-visible shared memory region (id 1, BAR 4) once mapped |

`RESOURCE_MAP_BLOB` places a `HOST3D` blob at a page-aligned offset of the
region, wholly inside it and overlapping no other mapping, and answers
`VIRTIO_GPU_RESP_OK_MAP_INFO` with `map_info` `VIRTIO_GPU_MAP_CACHE_CACHED`. The
contents survive `RESOURCE_UNMAP_BLOB` and a later map elsewhere; unmapped
offsets read as zero. `RESOURCE_UNREF` of a blob releases it, mapped or not.

| Request | Answer |
|---|---|
| `RESOURCE_CREATE_BLOB` shorter than its 56 bytes, or its entries | `ERR_INVALID_PARAMETER` |
| id 0, or an id a 2D resource or blob holds | `ERR_INVALID_RESOURCE_ID` |
| guest entries shorter than `size` | `ERR_INVALID_PARAMETER` |
| guest entries that are not guest RAM (the device would see a copy) | `ERR_UNSPEC` |
| host memory or blob slots exhausted | `ERR_OUT_OF_MEMORY` |
| `RESOURCE_MAP_BLOB` of an unknown id | `ERR_INVALID_RESOURCE_ID` |
| a guest blob, a blob already mapped, a misaligned offset, one past the region, or an overlap | `ERR_INVALID_PARAMETER` |
| `RESOURCE_UNMAP_BLOB` of an unknown id / an unmapped blob | `ERR_INVALID_RESOURCE_ID` / `ERR_INVALID_PARAMETER` |
| `RESOURCE_CREATE_2D` over a blob id | `ERR_INVALID_RESOURCE_ID` |
| `SET_SCANOUT_BLOB` | `ERR_INVALID_PARAMETER` |

A device reset releases every blob and mapping.

## Fence page (device -> guest)

Offset 0 of the fence resource. The device updates it from its worker thread;
the guest reads it from its interrupt path. 32 bytes.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `RVKF` = `0x464B5652` |
| 4 | u32 | flags | bit 0 `faulted`; no other bit |
| 8 | u64 | completed_fence | highest fence id whose work finished |
| 16 | u64 | progress | increments whenever the device makes progress (TDR heartbeat) |
| 24 | u32 | fault_code | 0 unless `faulted` |
| 28 | u32 | reserved | 0 |

Against what the guest has seen: `completed_fence` and `progress` never
decrease, `completed_fence` never exceeds the highest fence id submitted, and
once `faulted` is observed it stays set for the context's lifetime.
