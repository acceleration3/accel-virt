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
| 24 | u64 | features | bits below; unknown bits are ignored by the reader |
| 32 | u32 | vendor_id | `VkPhysicalDeviceProperties::vendorID` |
| 36 | u32 | device_id | `::deviceID` |
| 40 | u32 | device_type | `VkPhysicalDeviceType`: 0 other, 1 integrated, 2 discrete, 3 virtual, 4 cpu |
| 44 | u32 | reserved | 0 |
| 48 | u8[16] | device_uuid | `VkPhysicalDeviceIDProperties::deviceUUID` |
| 64 | u64 | local_memory | bytes in device-local heaps |
| 72 | u64 | shared_memory | bytes in host-visible, non-device-local heaps |
| 80 | u8[48] | name | UTF-8, NUL-terminated inside the field, every byte after the first NUL zero |

Each feature bit is something the host **proved**, not something it hopes for
(`src/device/probe/host_feasibility.cpp`):

| Bit | Name | What the device had to show |
|---|---|---|
| 0 | `host_visible_memory` | the device has a host-visible region at all: `hostmem=` is non-zero, so there is a BAR to place blobs in |
| 1 | `gpu_exported_memory` | memory allocated on the bound host GPU can be placed in that region and read back correctly through it (feature 039's `HostMemoryProbe`; on NVIDIA this works through dma-buf and not through an opaque fd) |
| 2 | `guest_memory_import` | the host GPU can address guest pages directly, `VK_EXT_external_memory_host` (feature 039's `GuestImportProbe`, which refuses anything that is not a direct mapping) |

A guest driver reports a CPU-visible memory segment over the region only when
bit 0 is set **and** it found shared memory region 1 in the device's PCI
capabilities: two answers from the device, because a segment reported without
storage behind it is how a guest ends up writing into memory that is not there
(feature 041).

Features are the only field where unknown values are tolerated: a newer device
may advertise a capability an older guest does not use.

## Context setup (guest -> device)

Sent in an rvk control command after `VIRTIO_GPU_CMD_CTX_CREATE` with context
init capset id 0x72. It binds the context to its ring, fence page and doorbell.
32 bytes.

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
| 24 | u32 | doorbell | the doorbell page, below the capset's `max_contexts` |
| 28 | u32 | reply_resource | 0 for no reply slot; otherwise different from `ring_resource` and `fence_resource` |

The checks that need the capset (major, sub_api membership, ring order range,
doorbell index) are a separate validation against a decoded capset; the decoder
checks what the bytes alone can prove.

## Capset and context commands (guest -> device)

A device realized with an executor library (see Execution) offers
`VIRTIO_GPU_F_CONTEXT_INIT` and `num_capsets` 1, and answers these virtio-gpu
requests itself (virtio 1.2 §5.7.6.8); without one it offers neither, and
refuses every request below with `ERR_INVALID_PARAMETER`. The capset carries
protocol 1.0, exactly the sub-APIs the executor offers, 64 contexts, ring orders 12 to 24,
`host_visible_memory` when the device has a hostmem region, and the host GPU the
device is bound to. The context setup travels as the whole command buffer of a
`SUBMIT_3D` to the context: `size` is 32 and the buffer is one setup. The ring
blob must hold at least `32 + 2^ring_order` bytes and the fence blob at least 32;
both stay referenced while the context lives, and `RESOURCE_UNREF` of either is
refused until `CTX_DESTROY` or a reset. The ring blob's header must already be
written: magic, the setup's order, head 0 and a tail at most `2^ring_order`.
A reply blob, when named, must hold at least 64 bytes and stays referenced with
the others. A successful setup connects the doorbell page it names to the
context, and holds a copy of the ring's data area and of the reply slot for
its worker.

| Request | Answer |
|---|---|
| `GET_CAPSET_INFO` index 0 | `OK_CAPSET_INFO`: id 0x72, max version 1, max size 128 |
| `GET_CAPSET` id 0x72, version 1 | `OK_CAPSET` with the 128-byte capset |
| any other index, id or version; a short request | `ERR_INVALID_PARAMETER` |
| `CTX_CREATE` with `context_init` exactly 0x72 and `nlen` at most 64 | `OK_NODATA` |
| `CTX_CREATE` of context 0 or a live context | `ERR_INVALID_CONTEXT_ID` |
| `CTX_CREATE` naming another capset, setting reserved bits, `nlen` over 64, or short | `ERR_INVALID_PARAMETER` |
| `CTX_CREATE` with every context slot taken, or no thread for it; a setup the host cannot stage | `ERR_OUT_OF_MEMORY` |
| `CTX_DESTROY` of a live context / any other | `OK_NODATA` / `ERR_INVALID_CONTEXT_ID` |
| `SUBMIT_3D` to a context that does not exist | `ERR_INVALID_CONTEXT_ID` |
| a setup naming a ring, fence or reply resource that is not a blob | `ERR_INVALID_RESOURCE_ID` |
| `size` not 32, the buffer short or malformed, a setup the capset does not allow, a blob too small for its role, a ring header that is not fresh, a doorbell another context holds, or a context already set up | `ERR_INVALID_PARAMETER` |
| `RESOURCE_UNREF` of a blob a context is bound to | `ERR_INVALID_PARAMETER`, the blob kept |

A device reset destroys every context, then releases every blob.

## Doorbells (guest -> device)

PCI BAR 0 of `virtio-gpu-rvk-pci` is a 32-bit memory BAR of `max_contexts`
pages of 4096 bytes (64 pages, 256 KiB), located by a virtio shared memory
capability (`VIRTIO_PCI_CAP_SHARED_MEMORY_CFG`) with id **0x72**. Page *n* is
the doorbell of the context whose setup named *n*.

A 4-byte store at offset 0 of a connected page wakes that context's worker; the
value is ignored. The store completes without the device's main loop (a KVM
ioeventfd; QEMU dispatches the same eventfd itself without KVM). Any other
width or offset, and any page no context holds, does nothing; reads return 0.
A store is a wake-up, not a count: the worker consumes everything published
since it last looked, so one store after several batches is enough.

## Batches (guest -> device, in the ring)

The ring's bytes are a sequence of batches. Each starts with a 16-byte header
and occupies the header and its payload rounded up to a multiple of 8 bytes.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `RVKB` = `0x424B5652` |
| 4 | u32 | length | payload bytes following the header; header and payload fit in `2^order` |
| 8 | u64 | fence_id | >= 1, greater than the previous batch's on this context |

A batch completes its fence. The producer assigns fence ids: a user-mode driver
its own, the kernel driver's submission path maps dxgkrnl's to them.

**Consumption.** On every wake the device reads the ring header, judges the
tail against its own history (see the ring header's rules; the device's own
head, not the header's head field, is the one judged and published), then
consumes batch after batch from its head while a whole header and then a whole
batch lie before the tail, publishing head after each. A partly published batch
waits for a later wake. A header that is malformed or changes order, a tail
that moves backwards or runs more than `2^order` ahead of head, or a batch with
a bad magic, a length that cannot fit the ring, fence 0 or a fence not greater
than the last, stops the context: the device logs the reason once and consumes
nothing more for it until `CTX_DESTROY` or a reset. Other contexts carry on.

## Execution (guest -> device, in batches)

A batch's payload is a sequence of commands, each framed by the header below,
filling the payload exactly:

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | id | top 16 bits: the API - 0 transport, 1 Vulkan, 2 OpenCL, 3 OpenGL |
| 4 | u32 | size | low 24 bits: the command's bytes including this header, at least 8; top 8 bits: flags, bit 0 *generate reply*, the rest 0 |

A command id's low 16 bits name the command within its API. Which id each
command has is not written out here - there are close to two thousand of them -
but they are not free either: they live in `tools/codegen/profiles/*_ids.toml`,
one file per API, and the generator appends a new command at the next free id
and never moves an existing one. `tools/check_protocol_ids.py` is the ctest gate
that the ids in those files stay inside the range this table gives their API.
Vulkan owns `0x1____`, OpenCL `0x2____`, OpenGL `0x3____`.

The device copies the payload once, then walks it: a command of the
context's sub-API runs through the context's executor; a transport command
(none is defined yet), another API's, or one the executor does not know is
skipped. The answer of the last command in the batch whose *generate reply*
flag is set is written to the reply slot before head passes the batch, so a
producer waiting for head reads a finished answer. A command framed wrong,
trailing bytes too short for a header, a reply asked for with no reply slot,
a payload the executor cannot decode, or a lost host device stops the context
(see Consumption) and the batch does not complete.

### Reply slot (device -> guest)

Offset 0 of the reply resource; the answer follows at offset 32. 32 bytes.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `RVKY` = `0x594B5652` |
| 4 | u32 | status | 0 ok: `size` bytes follow; 1 too large: nothing follows |
| 8 | u64 | fence_id | the batch the answer belongs to, >= 1 |
| 16 | u32 | size | ok: equal to `required`; too large: 0 |
| 20 | u32 | required | the answer's length (4 GiB and over reads 0xFFFFFFFF); too large: non-zero |
| 24 | u64 | reserved | 0 |

The slot holds `reply blob size - 32` bytes of answer, at most 16 MiB. A batch
asking for no answer leaves the slot as it was.

### The executor ABI (device <-> host library)

Every call must return: `CTX_DESTROY` and a reset wait for a worker's call to
finish.


`src/device/abi/rvk_executor.h`, now version 2, is what runs a context's commands on
the host's API: `sub_apis`, `create(sub_api, gpu_uuid)`, `destroy`, and
`execute(id, payload, size, reply, capacity, &reply_size)` answering executed,
unknown, undecodable or device lost. The QEMU device loads the library its
`executor` property names and calls `rvk_executor_ops` for the table; a library
that does not load, lacks the symbol, or has another ABI version fails realize.
Each context's executor is created and destroyed on the context's worker, and
only that thread calls it. The executor that answers for real is this
project's own, built on the generated server dispatch in
`src/proto/generated/*/` over host API backends (feature 042); the qemu
configuration builds `rvk_echo_executor` for the tests.

**Version 2 adds a memory channel**, because version 1 had none: the only bytes
an executor could see were one command's payload, so a command naming a blob had
nothing to name it with. It adds `import_kinds`, `import_blob` and
`release_blob`, and changes nothing else. The device accepts both versions and a
version 1 library simply imports nothing; a table claiming version 2 and
carrying only part of it is refused at realize rather than found out one blob
later on a worker.

A blob crosses as a tagged handle - a host pointer or a dma-buf, whichever the
API can take without a copy - with no offset, because the device hands over a
handle naming exactly the range and OpenCL's own import has no offset property.
`import_kinds` is asked per sub-API and per GPU, on the main loop, while the
capset is being described; `import_blob` and `release_blob` run on the context's
worker like `execute`. The capset's `feature_guest_memory_import` bit means both
halves are true: this device can build a handle, and the library will take one.

Which handle kinds work is a property of the host, not of the protocol: a host
pointer imports into Vulkan on every GPU measured, while OpenGL and OpenCL take
a dma-buf and only on some drivers. Where neither works the guest is copied at
the boundary and the capset bit is clear.

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

The fault codes, which say where to look rather than only that something
failed: 1 ring header, 2 ring tail, 3 batch header, 4 batch order, 5 command
framing, 6 undecodable, 7 device lost, 8 no executor, 9 broken executor. The
last is a library answering a value the executor ABI does not define, and it is
separate from device lost on purpose - folded together, a bug in an executor
reads as a GPU fault and is chased in the wrong place.

Against what the guest has seen: `completed_fence` and `progress` never
decrease, `completed_fence` never exceeds the highest fence id submitted, and
once `faulted` is observed it stays set for the context's lifetime.

**When the device writes it.** At the context setup, before the ring is
attached: magic, flags 0, completed 0, progress 0, fault code 0. Then from the
context's worker: after every command it executes, progress moves on (a
heartbeat: a long batch reads as busy, not hung); after every batch it
completes, completed is that batch's fence and progress moves on again; and
once, if the context faults, the page reads `faulted` with the last completed
fence, the progress so far and the fault code. Nothing is written after a
fault. Every write is the whole page from the device's own counters; the
device never reads the page back, so a page the guest overwrites is rewritten
by the next write.

| Fault code | Reason |
|--:|---|
| 1 | ring header malformed, or its order changed |
| 2 | ring tail moved backwards, or more than the ring ahead of head |
| 3 | batch header malformed, or the batch cannot fit the ring |
| 4 | batch fence not greater than the last |
| 5 | commands framed wrong, or an answer asked for with no reply slot |
| 6 | a payload the executor cannot decode |
| 7 | the host device was lost |
| 8 | the context's executor could not be created |

A faulted context's executor is destroyed at once, on its worker; the context,
its doorbell and its blobs stay until `CTX_DESTROY` or a reset.

**Recovery.** The guest's engine reset destroys the context (`CTX_DESTROY`),
writes a fresh ring header, and creates and sets up a context again - the same
id, blobs and doorbell may be reused - which starts healthy from progress 0.
An adapter reset is a virtio reset: every context is destroyed and a new
driver starts from nothing. A context the guest wants to stop without a fault
(preemption) needs no command: the guest stops publishing its tail.

## The driver's sequence

The order a guest driver follows (`src/core/rvk/`), checked end to end against
the device core by the loopback suite (`tests/loopback/`):

1. **Capset.** `GET_CAPSET_INFO` for index 0, then `GET_CAPSET` for the id and
   version it names. A capset that does not decode, is not `0x72`, or is of
   another major ends the query; nothing below starts without one.
2. **Memory.** A ring of `32 + 2^order` bytes rounded up to pages with a fresh
   header, a fence page and a reply page, each physically contiguous, zeroed.
3. **Setup.** `CTX_CREATE` with capset `0x72`, then `RESOURCE_CREATE_BLOB`
   (guest memory, one entry) for the ring, the fence page and the reply slot,
   then the context setup in `SUBMIT_3D`. Each is sent when the one before is
   answered. Any answer other than `OK_NODATA`, or one that does not decode,
   fails the setup; a step whose answer did not decode is counted as done,
   since the device may have acted on it.
4. **Submission.** The producer writes batches past the tail; the driver checks
   the descriptor starts at its tail, fits the ring, carries a fence greater
   than the last, and that the device's head is not past the tail it published
   and leaves room (`no_space` otherwise, nothing published). Then it stores
   the tail and writes the doorbell page.
5. **Completion.** On the context's fence interrupt the driver copies the fence
   page once and judges it against what it submitted (the rules under "Fence
   page"). A page or head that breaks a rule fails the context: it submits and
   serves nothing more until torn down.
6. **Teardown.** `CTX_DESTROY` if the context was created, then
   `RESOURCE_UNREF` for each blob created, in order; then the memory is freed.
   After a virtio reset the driver abandons its contexts - the device holds
   none - and frees their memory without requests.

## Fence queues and interrupts (device -> guest)

`virtio-gpu-rvk-pci` has 66 virtqueues: control (0), cursor (1), and a fence
queue per doorbell page, queue `2 + n` for doorbell `n`. A fence queue carries
no requests - a kick is ignored - and exists so the driver can give each
context's interrupts a vector with `queue_msix_vector`; queues may share a
vector, and one left at `VIRTIO_MSI_NO_VECTOR` raises nothing. The PCI
function keeps virtio-gpu's `vectors` property (default 3), so a driver that
wants a vector per context starts QEMU with more.

When a worker's service completes at least one batch, it writes the page(s)
and then interrupts once on the context's fence queue. The interrupt goes
through the queue's guest notifier, connected when the driver sets
`DRIVER_OK`: with KVM an irqfd on the queue's MSI-X vector, masking handled by
virtio-pci (a masked vector's pending bit is set); without MSI-X, the ISR's
queue bit and INTx. A reset destroys every context before the notifiers are
released. Nothing is raised before `DRIVER_OK`; the page is still written.
