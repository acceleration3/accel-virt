# The av wire protocol

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

`major.minor`, both `u16`. This document is **1.1**.

Two peers are compatible when their majors are equal. The negotiated version is
`major.min(local minor, peer minor)`. A structure's layout depends only on the
major, with one exception: minor 1 appends the context setup's options (feature
050), and the setup's minor says which layout it has. The driver writes the
setup at the negotiated minor, so a device at minor 0 is sent the 32-byte setup
and never an option.

## Capset (device -> guest)

virtio-gpu capset id **0x72**, capset version 1, returned by
`VIRTIO_GPU_CMD_GET_CAPSET`. One per device instance, i.e. one per host GPU
(`docs/kb/11-implementation-plan.md`, "One adapter per host GPU"). 128 bytes.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `AV1` = `0x00315641` |
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
| 80 | u8[48] | name | UTF-8, NUL-terminated inside the field, every byte after the first NUL zero: the adapter's name (below) |

`name` is what the guest calls the adapter in Device Manager, WMI and DXGI:
`VkPhysicalDeviceProperties::deviceName` without a trailing parenthesised
group set off by a space, which Mesa's drivers append to name themselves
("AMD Radeon RX 9070 XT (RADV GFX1201)" is sent as "AMD Radeon RX 9070 XT"),
cut to fit at a UTF-8 character boundary (`src/device/gpu/adapter_name.hpp`).
The PCI database's name is not used: pci.ids names the silicon ("Navi 48
[Radeon RX 9070/9070 XT/9070 GRE]"), not the product.

The PCI function carries the rest of the host GPU's identity. When the bound
GPU reports `VK_EXT_pci_bus_info` and sysfs describes its function,
virtio-gpu-av-pci presents that function's vendor, device and revision as its
own (`src/device/gpu/host_pci_identity.hpp`); the subsystem stays `1af4:5256`,
which is how the driver package and the guest's programs recognise the device.
A GPU with no PCI function (llvmpipe) leaves virtio-gpu's `1af4:1050`.

Each feature bit is something the host **proved**, not something it hopes for
(`src/device/feasibility/host_feasibility.cpp`):

| Bit | Name | What the device had to show |
|---|---|---|
| 0 | `host_visible_memory` | the device has a host-visible region at all: `hostmem=` is non-zero, so there is a BAR to place blobs in |
| 1, 2 | unused | feature 039's probes of exporting host GPU memory and of importing guest pages. Every host-visible byte is host GPU memory now (docs/host-memory.md), so neither is a choice; a device never sets them |
| 3 | `dmabuf_scanout` | a display head's console can show a dma-buf, so a blob or an exported image may be scanned out instead of transferred (feature 058) |
| 4 | `present_engine` | the device moves queued presents on its own thread (see "Present engine") |
| 5 | `present_from_image` | a queued present's source may be a host image, which this device can read (see "Present engine"); set only with bit 4 |
| 6 | `raises` | the fence page counts out-of-band raises in `raised`, so the driver can wake a process that waits on them (see "Fence page" and "Wake event record") |
| 7 | `scanout_planes` | the device composes a head's overlay planes (see "Scanout planes") |

Bit 3 is the one bit the device cannot decide when it describes the capset, and
the reason is QEMU's own order: `qemu_init_displays()` runs after
`qmp_x_exit_preconfig()` has realized every device, and it is the display
backend - not the device - that gives a console its GL context. So it is
decided when the guest first reads the capset, once, and the answer the guest
was given is the one that stands for the life of the device.

A guest driver uses the region as its host window (docs/host-memory.md) only
when bit 0 is set **and** it found shared memory region 1 in the device's PCI
capabilities: two answers from the device, because a window used without
storage behind it is how a guest ends up writing into memory that is not there
(feature 041). VidMm is never told of the region. A driver with no window has
no host-visible memory at all: nothing falls back to guest RAM.

Features are the only field where unknown values are tolerated: a newer device
may advertise a capability an older guest does not use.

## Context setup (guest -> device)

Sent in an av control command after `VIRTIO_GPU_CMD_CTX_CREATE` with context
init capset id 0x72. It binds the context to its ring, fence page and doorbell.
32 bytes at minor 0; 48 at minor 1 and later, and a setup whose length is not
its minor's is malformed.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `AVC` = `0x00435641` |
| 4 | u16 | major | the negotiated version |
| 6 | u16 | minor | |
| 8 | u32 | sub_api | exactly one bit, one of the capset's `sub_apis` |
| 12 | u32 | ring_resource | virtio-gpu resource id, non-zero |
| 16 | u8 | ring_order | within the capset's [min, max] |
| 17 | u8[3] | reserved | 0 |
| 20 | u32 | fence_resource | non-zero, different from `ring_resource` |
| 24 | u32 | doorbell | the doorbell page, below the capset's `max_contexts` |
| 28 | u32 | reply_resource | 0 for no reply slot; otherwise different from `ring_resource` and `fence_resource` |
| 32 | u8 | gl_major | minor 1: 1..4 for OpenGL; 0 means the host's default context, and then every option below is 0 |
| 33 | u8 | gl_minor | at most `gl_major`'s highest minor (1.5, 2.1, 3.3, 4.6) |
| 34 | u8 | gl_profile | 0 none, 1 core, 2 compatibility |
| 35 | u8 | gl_flags | bit 0 debug, 1 forward compatible, 2 robust access, 3 lose context on reset; 4-7 zero |
| 36 | u32 | share_context | the device context id of a bound OpenGL context to share objects with, or 0 |
| 40 | u8[8] | reserved | 0 |

Options belong to OpenGL: a setup of another sub-API carries all eight option
bytes zero. The device refuses (`ERR_INVALID_PARAMETER`) a share context that
is not another bound OpenGL context of the device.

The checks that need the capset (major, sub_api membership, ring order range,
doorbell index) are a separate validation against a decoded capset; the decoder
checks what the bytes alone can prove.

## Capset and context commands (guest -> device)

A device realized with an executor library (see Execution) offers
`VIRTIO_GPU_F_CONTEXT_INIT` and `num_capsets` 1, and answers these virtio-gpu
requests itself (virtio 1.2 §5.7.6.8); without one it offers neither, and
refuses every request below with `ERR_INVALID_PARAMETER`. The capset carries
protocol 1.1, exactly the sub-APIs the executor offers, 64 contexts, ring orders 12 to 24,
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
| a setup naming a ring, fence or reply resource that is not a `GUEST` blob | `ERR_INVALID_RESOURCE_ID` |
| `size` not 32, the buffer short or malformed, a setup the capset does not allow, a blob too small for its role, a ring header that is not fresh, a doorbell another context holds, or a context already set up | `ERR_INVALID_PARAMETER` |
| `RESOURCE_UNREF` of a blob a context is bound to | `ERR_INVALID_PARAMETER`, the blob kept |
| `RESOURCE_UNREF` of a host blob an IMPORT_BLOB still holds | `OK_NODATA`: the blob goes, and the executor library keeps its memory until the import does |

A device reset destroys every context, then releases every blob.

## Doorbells (guest -> device)

PCI BAR 0 of `virtio-gpu-av-pci` is a 32-bit memory BAR of `max_contexts`
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
| 0 | u32 | magic | `AVB` = `0x00425641` |
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

Feature 052 appended ten to OpenCL's range, `0x2006A` to `0x20073` in this
order: `clEnqueueMapBuffer`, `clEnqueueUnmapMemObject`, `clSVMAlloc`,
`clSVMFree`, `clEnqueueSVMMap`, `clEnqueueSVMUnmap`, `clEnqueueSVMMemcpy`,
`clEnqueueSVMMemFill`, `clSetKernelArgSVMPointer`, `clSetEventCallback`. Two of
them are shapes the wire did not have before. An SVM address crosses as a
`uint64_t` and the device resolves it against the allocations that context's
own `clSVMAlloc` made; an address, or an address and extent, that does not lie
inside one live allocation does not decode, and the context faults with code 6
rather than the host following a pointer a guest chose. And
`clSetEventCallback` carries no function pointer: a guest address is not
something the host can call, so the command says only which event and which
status, the device registers a callback of its own with the host, and the guest
is told through the fence page's out-of-band raise and asks what completed.

The device copies the payload once, then walks it: a command of the
context's sub-API runs through the context's executor; a transport command the
table below defines runs in the device itself; another API's command, any other
transport id, or one the executor does not know is skipped. The answer of the
last command in the batch whose *generate reply* flag is set is written to the
reply slot before head passes the batch, so a producer waiting for head reads a
finished answer. A command framed wrong, trailing bytes too short for a header,
a reply asked for with no reply slot, a transport command carrying a body it
does not have, a payload the executor cannot decode, or a lost host device stops
the context (see Consumption) and the batch does not complete; the first four
are fault code 5, command framing.

### Transport commands (API 0)

The device answers these itself and never hands one to the executor library,
whichever library is loaded, so they behave the same under every executor.

| Id | Command | Body | Answer |
|--:|---|---|---|
| `0x00000001` | NOP | none | empty: with *generate reply*, a reply of size 0 for the batch's fence |
| `0x00000002` | IMPORT_BLOB | 8 bytes: u32 resource_id, u32 reserved | 16 bytes: u32 status, u32 reserved, u64 device_memory |
| `0x00000003` | RELEASE_BLOB | 8 bytes: u64 device_memory | empty |
| `0x00000004` | SHARE_OBJECT | 32 bytes: u64 object_id, u32 kind, u32 reserved, u64 wire_handle, u64 size | 16 bytes: u32 status, u32 reserved, u64 zero |
| `0x00000005` | OPEN_SHARED_OBJECT | the same 32 bytes, wire_handle 0 | 16 bytes: u32 status, u32 reserved, u64 wire_handle |
| `0x00000006` | RELEASE_SHARED_OBJECT | the same 32 bytes, wire_handle 0 | 16 bytes: u32 status, u32 reserved, u64 zero |

A NOP does nothing but complete, which makes it the round trip a guest times to
measure the transport alone rather than a backend (feature 044). It moves the
context's progress like any command run.

IMPORT_BLOB hands one host blob, whole, to the context's executor and answers
the wire handle the guest names that memory by from then on: for Vulkan the
`VkDeviceMemory`, for a Direct3D 12 heap or the Direct3D 11 slice store the
handle `CreateHeap12` and `BindBufferSlice` name it by (docs/host-memory.md).
The resource must be a `HOST3D` blob made for this context - its
`RESOURCE_CREATE_BLOB` named the context in `ctx_id` (see Blob resources) - so
a context imports only memory its own process opened. A guest blob, another
context's host blob or any other id is refused. The device hands the library
the blob's resource id, which is the key the library allocated its memory
under, and the context's API takes that allocation: Vulkan imports its dma-buf
as the guest's `VkDeviceMemory`, vkd3d-proton opens a Direct3D 12 heap from
it, DXVK's device imports the slice store as a `VkBuffer`, and an arena is read
and written through the memory source's own mapping. An import holds a
reference to the memory, so `RESOURCE_UNREF` of a host blob is never refused:
the window range and the blob go, and the memory stays until RELEASE_BLOB,
`CTX_DESTROY` or a reset ends the import.

An OpenGL context imports once (feature 050): its guest ICD opens one
`readback` chunk as its blob arena and imports it, and the OpenGL backend
adopts that memory as the context's region - bulk data a GL command names by
offset into the arena (a `glReadPixels` destination, a texture upload, a
buffer map's copy) is read and written there in place. A second import into
the same context is refused; the wire handle it answers is the region's
address, which no GL command names. A Direct3D session's first import is its
arena, adopted the same way; every later one is a CPU-visible Direct3D 12 heap
or the slice store, kept apart from the region (`ImportedMemories`).

A body of any length but its command's is command framing, fault 5, as for a
NOP with a body. Every other refusal is an answer, and the context runs on:

| status | IMPORT_BLOB refused because |
|--:|---|
| 0 | not refused: `device_memory` names the import, never 0 |
| 1 | `resource_id` is 0, or the reserved field is not 0 |
| 2 | the resource is no host blob made for this context |
| 3 | the context has no executor, or its API will not take the memory |
| 4 | the context holds 4096 imports, or the host has no memory for the table of them |

A refused import answers `device_memory` 0. RELEASE_BLOB of a handle the context
never imported, or already released, answers empty and does nothing.

#### Shared objects (feature 053)

The three shared-object commands give one host resource an identity the whole
device agrees on, so two contexts - of two guest devices, in two guest
processes - naming one id hold one host object with two references. The id is
not the guest's to invent: the miniport mints it into the allocation private
driver data of a shareable allocation, and that data is what dxgkrnl carries
from the creating process to the opening one (docs/kb/04-memory.md). `kind` is
1 memory, 2 a monitored fence, 3 a native fence; `size` is whole non-zero pages
for memory and 0 for either fence.

The device only routes them: the executor library keeps every shared object,
of every API, in one table per host GPU (`HostShares`, docs/shared-surfaces.md
§0), under the device's domain and the id.

**SHARE_OBJECT** publishes an object this context's guest already made. The
library's session for this context exports a descriptor of `wire_handle` and
the library records it under `object_id`, held by the guest. The same context
publishing the same handle under the same id again answers ok and changes
nothing; a Direct3D texture the context made shared is described and exported
by its first SHARE_OBJECT.

**OPEN_SHARED_OBJECT** has the context's session import that descriptor and
answers the handle it made of it - a `VkDeviceMemory`, a GL memory object name
- which the guest names in that API's own commands from then on. The context
holds the object until it releases it or is destroyed. A context opening an id
it already holds, or one it published, is refused: a second handle for one
object in one context is one the guest could free twice.

**RELEASE_SHARED_OBJECT** destroys what this context made of the object and
drops its hold. An id this context never held answers ok and does nothing, as
RELEASE_BLOB does. A context destroyed lets go of everything it still holds.
The object lives while the guest holds it (0x02F1, below) or any context does,
so it outlives the context that published it, which is the whole point.

Only Vulkan exports a descriptor; OpenGL imports one and exports none, and
OpenCL does neither yet. A guest asking an executor for what its API cannot do
is answered status 4.

| status | a shared-object command refused because |
|--:|---|
| 0 | not refused: an OPEN answers a non-zero `wire_handle`, the other two answer 0 |
| 1 | `object_id` is 0, `kind` is not one of the three, a reserved field is not 0, `size` is not what its kind has, or a `wire_handle` travelled with a command that allows none |
| 2 | OPEN: an object nothing in this device shares under the id |
| 3 | the kind or the size disagrees with the object shared under the id |
| 4 | the context has no executor, or the library refused: a wire handle its session never made, an API that exports or imports nothing of the kind, no memory for one more |
| 5 | not answered: there is no count of objects past which one is refused |
| 6 | SHARE: the id is already shared otherwise. OPEN: this context already holds it, or published it |

A refused command answers `wire_handle` 0, and a guest that sees one on a
refusal must not believe it: the answer's own decode refuses that shape.

#### Allocation private driver data

Private data of exactly 56 bytes is an allocation that is not shared, which is
every allocation before feature 053. Private data of 96 bytes is an
`AllocationDescriptor` followed by a 40-byte `SharedObjectRecord`, which is an
allocation asking for an identity: `DxgkDdiCreateAllocation` mints the id and
the adapter's own number into the record it was handed and writes both back, so
the value the runtime keeps is the one the opening process is given. Any other
length is refused. At `DxgkDdiOpenAllocation` the miniport believes nothing it
did not write: an id it never minted, an id another adapter minted, a record
that does not decode, and a kind or a size that disagrees with the object are
each refused with nothing opened.

#### The guest's release of a shared object (control queue)

What a context shares lives as long as the guest holds the allocation its id
was minted over, not as long as the context: on Windows a window's surface
outlives its creator's handle while DWM still composes it, and DWM opens it
again as the creator destroys it. dxgkrnl calls `DxgkDdiDestroyAllocation` on a
shared allocation only after the last process that made or opened it has
closed it, so that destroy is the one moment the guest knows the id is gone
everywhere, and the miniport then tells the device on the control queue:

| Offset | Size | Field | |
|--:|--:|---|---|
| 0 | 24 | header | a `virtio_gpu_ctrl_hdr`, type `0x02F1`, context 0, no fence |
| 24 | 8 | object_id | the id the miniport minted; never 0 |

The device hands the id to the executor library in its domain
(`release_shared`), which drops the guest's hold on what was shared under it -
a feature 053 object, a Direct3D 11 surface - and frees it once no context
holds it either. It answers `OK_NODATA`, an id it never held included, and
`ERR_INVALID_PARAMETER` for a request that is short or names id 0. The miniport
sends it synchronously, after its allocation table's lock is dropped; a device
that is not there is told nothing, since a reset already let go of everything.
A device reset releases every id at once, which is id 0 inside the device and
the library; what a context still holds stays its own, but no longer answers
to the id, which the guest mints again.

#### The present copy (control queue)

A present's pixels - dxgkrnl's own blts and fills, and an application's
blt-model present into its window's redirection surface - move between guest
pages. The miniport's CPU sees those pages uncached, so moving them there cost
a 1280x800 blt thirty milliseconds; the host reads and writes guest RAM through
its own cache. The miniport clips every rectangle to both surfaces first and
sends what is left as regions:

| Offset | Size | Field | |
|--:|--:|---|---|
| 0 | 24 | header | a `virtio_gpu_ctrl_hdr`, type `0x02F2`, context 0, no fence |
| 24 | 4 | operation | 1 blt, 2 colour fill (`PresentOperation`) |
| 28 | 4 | flags | bit 0: swap each pixel's first and third byte - the two surfaces' bytes run in opposite orders; blt only. Others 0 |
| 32 | 4 | colour | the four bytes a fill writes to each pixel; 0 for a blt |
| 36 | 4 | region_count | 1 to 256 |
| 40 | 4 | source_entries | a blt's: 1 to 16384; a fill's: 0 |
| 44 | 4 | destination_entries | 1 to 16384 |
| 48 | 4 | source_pitch | bytes between a blt's source rows, a whole number of four-byte pixels, not 0; a fill's: 0 |
| 52 | 4 | destination_pitch | the same, for the destination |
| 56 | 16 each | source entries | virtio memory entries: u64 address, u32 length, u32 0 |
| | 16 each | destination entries | |
| | 24 each | regions | u32 source_x, source_y, destination_x, destination_y, width, height, in pixels; width and height not 0 |

A surface is its entries' bytes end to end, and pixel (x, y) of it is the four
bytes at `y * pitch + x * 4`. The fixed part travels in a control slot; the rest
is too large for one, so the chain carries it as a second read-only buffer of
the driver's own contiguous memory, and the device reads the two as one
request. The miniport waits for the answer - asks on the control queue take
turns - and the pixels have moved when it comes: `OK_NODATA`.

`ERR_INVALID_PARAMETER` answers a fixed part that breaks a rule above, entries
cut short or naming what is not guest RAM, a region cut short, and a region
with a row that is not wholly inside either surface; that region's rows before
the one outside, and every region before it, have moved - into the
destination's own entries, which is all a request can make the device write.
`ERR_OUT_OF_MEMORY` answers a host with no memory to map the entries. A present
the device refuses, does not answer, or the driver cannot hand it - more pages
than its buffer holds - the miniport moves itself, as it did before this
request.

#### The display's modes (control queue)

The resolutions the display offers, any sizes and not only the standard ones,
are the host's to set: the device's `modes` property (`WxH` entries between
commas, the first preferred, such as `2560x1440,1920x1080,1000x600`), given on
QEMU's command line or changed over QMP while the guest runs; or, set to
nothing, the modes of the monitors connected to the host, as
`/sys/class/drm/<card>-<connector>/modes` lists them - interlaced ones and
those under 640x480 left out, the largest monitor's best mode preferred. The
miniport asks at start and again after each display event:

| Offset | Size | Field | |
|--:|--:|---|---|
| 0 | 24 | header | a `virtio_gpu_ctrl_hdr`, type `0x02F3`, context 0, no fence |

and the device answers:

| Offset | Size | Field | |
|--:|--:|---|---|
| 0 | 24 | header | type `0x11F3` |
| 24 | 4 | count | 1 to 32 |
| 28 | 4 | preferred | the index of the mode the guest shows unless its user picks another; below `count` |
| 32 | 8 each | modes | u32 width, u32 height, each 1 to 16384; no mode twice |

A device with no modes to offer - none set and no monitor - answers
`ERR_UNSPEC`, and so does one from before this request; the miniport then
offers the standard modes that fit `GET_DISPLAY_INFO`'s rectangle, as it did
before. The device keeps that rectangle at the preferred mode, and a change of
modes while the guest runs sets `VIRTIO_GPU_EVENT_DISPLAY` and notifies the
configuration change, which is when the miniport asks again, clears the event
and has Windows read the monitor's modes afresh. A mode the miniport cannot
describe in its EDID's detailed timing - an edge past 4095, or a pixel clock
past 655.35 MHz at 60 Hz - is not offered.

### Reply slot (device -> guest)

Offset 0 of the reply resource; the answer follows at offset 32. 32 bytes.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `AVY` = `0x00595641` |
| 4 | u32 | status | 0 ok: `size` bytes follow; 1 too large: nothing follows |
| 8 | u64 | fence_id | the batch the answer belongs to, >= 1 |
| 16 | u32 | size | ok: equal to `required`; too large: 0 |
| 20 | u32 | required | the answer's length (4 GiB and over reads 0xFFFFFFFF); too large: non-zero |
| 24 | u64 | reserved | 0 |

The slot holds `reply blob size - 32` bytes of answer, at most 16 MiB. A batch
asking for no answer leaves the slot as it was.

### The executor ABI (device <-> host library)

`src/device/abi/av_executor.h`, version 20, is what runs a context's commands
on the host's API, and the library behind it is where every object the
contexts share is kept. The QEMU device loads the library its `executor`
property names and calls `av_executor_ops` for the table; a library that does
not load, lacks the symbol, has another ABI version or a null in its table
fails realize. There is one version: the device, the library and the guest
drivers are built together, and an older table is a build to fix, not one to
serve. The executor that answers for real is this project's own, built on the
generated server dispatch in `src/proto/generated/*/` over host API backends
(feature 042); the qemu configuration builds `av_echo_executor` for the tests.

- **Domain.** A device is a domain of the library: `open_domain(gpu_uuid)`
  answers it as the device starts (in QEMU, or in the renderer that runs its
  contexts), `close_domain` ends it as the device is destroyed. The guest
  mints its shared ids per device, and the domain keeps two devices' ids
  apart on one GPU. Every call below that is not an executor's names the
  domain, not the GPU.
- **Executors.** `create(sub_api, options, share)` makes a context's executor
  in the domain `options` names, with an OpenGL context's version, profile and
  flags (feature 050) and the executor of the context it shares objects with,
  which the device resolves from the setup's `share_context` and keeps alive
  until the create returns; other sub-APIs refuse options and shares. Each
  executor is created, run (`execute`, answering executed, unknown,
  undecodable or device lost; `end_batch`, `poll`, `gpu_progress`) and
  destroyed on its context's worker, and only that thread calls it.
- **Signals.** `set_signal(executor, signal, context)` (feature 052) runs the
  other way: the library calls `signal(context)` when the host API finishes
  something on a thread the device never gave it - the host's OpenCL driver
  calling back an event callback. Nothing about what completed crosses; the
  call must do almost nothing and must not call back into the table. Because
  OpenCL cannot unregister a callback, a signal may arrive after the executor
  it names is destroyed: the `context` the device hands over lives as long as
  the device does, and a signal for a context that is gone is tolerated.
- **Memory.** `allocate_memory`, `free_memory`, `import_blob`, `release_blob`
  and `imported_handle` are the memory channel (docs/host-memory.md), below.
- **Shared objects** (feature 053). `share_object(executor, id, kind, size,
  wire_handle)`, `open_object(executor, id, kind, size, &wire_handle)` and
  `close_object(executor, id)` are the three transport commands above, run on
  the context's worker and answered `AV_SHARED_DONE`, `_UNKNOWN`,
  `_MISMATCHED`, `_CONFLICT` or `_REFUSED`; an open answering done with no
  handle is refused and closed. `release_shared(domain, id)` is the guest's
  release (0x02F1) on the main loop, id 0 a reset.
- **Images.** `read_image` and `export_image` read and export what is shared
  under a key for QEMU's display; `map_frame` and `unmap_frame` read the
  frames a present staged (docs/shared-surfaces.md); `export_surface` and
  `sync_surface` hand a shared surface to a process outside QEMU - a dma-buf
  the caller owns, whose modifier is `AV_EXPORT_MODIFIER_IMPLICIT`, and how
  many copies into it ran with a sync_file for them (docs/external-export.md).
  The frame and surface calls run on any thread.

Every call must return: `CTX_DESTROY` and a reset wait for a worker's call to
finish.

The library is the source of every byte the guest CPU and a host GPU share
(docs/host-memory.md). `allocate_memory(domain, key, size, request)` answers
`size` bytes of the memory a `HostMemoryRequest` names - `upload`, `readback`
or one Vulkan memory type - on the domain's GPU, as an `AvHostMemory`: a
dma-buf the device owns and maps into the hostmem region, its size, and how
the host maps it (`AV_MEMORY_WRITE_BACK` or `AV_MEMORY_WRITE_COMBINED`), which
is how the guest must map it too. The device allocates under the host blob's
resource id and `free_memory(domain, key)` lets go of it when the guest
releases the blob; both run on the main loop. `import_blob(executor, blob_id,
memory_key)` makes the memory under a key usable by one executor's commands
and holds it until `release_blob` or `destroy`, so memory the guest released
while an import still has it lives until the import goes. Both run on the
context's worker like `execute`. No guest page is ever handed to the library,
and no import kind is negotiated: every import is the library's own
allocation.

**What the guest is told is host visible** is what the memory source can
serve. The executor answers `vkGetPhysicalDeviceMemoryProperties` (all three
spellings) with `HOST_VISIBLE`, and `HOST_COHERENT` and `HOST_CACHED` with it,
cleared from every type the GPU's memory source could not allocate, export as
a dma-buf and map through the export when it first tried that type - a driver
can claim a type exports and then refuse it - since the guest maps
host-visible memory through the window and has no other way to reach it. The
mask is learnt once per physical device (`ImportableTypes`). Buffers and
images made on the guest's device carry dma-buf external-memory create info
where the driver reports the create importable, unless the guest's chain names
its own, so any of them can bind memory the source made.

### The export socket (device <-> a host process)

Not the guest's: the socket a device's `export-socket` property names, on
which a process on the host is handed shared surfaces as dma-bufs. Its own
messages and version, in docs/external-export.md.

## Ring header (shared, both write)

Offset 0 of the ring resource; the ring's data area follows at offset 32 and
holds `2^order` bytes. 32 bytes.

| Offset | Type | Field | Writer | Rule |
|--:|---|---|---|---|
| 0 | u32 | magic | guest, once | `AVR` = `0x00525641` |
| 4 | u8 | order | guest, once | 12..24 |
| 5 | u8[3] | reserved | | 0 |
| 8 | u64 | tail | guest driver, at submission | total bytes produced and submitted |
| 16 | u64 | head | device | total bytes consumed |
| 24 | u64 | reserved | | 0 |

A byte's position in the data area is `counter & (2^order - 1)`. At every
observation `head <= tail <= head + 2^order`, and neither counter is smaller
than the last value observed. Each side trusts only its own counter and
validates the other's against its last observation.

The producer writes batch bytes past the tail but never the tail itself: the
device consumes up to whatever tail it finds on any wake, so a tail stored
before the driver checked the submission lets the device's head pass the tail
the driver published, and the driver fails the context for it.

## Submission descriptor (guest -> device)

One batch of ring bytes and the fence that completes it; carried as the DMA
buffer of a submission. 32 bytes.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `AVS` = `0x00535641` |
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
| 0 | u32 | magic | `AVA` = `0x00415641` |
| 4 | u16 | kind | 1 shared primary, 2 shadow, 3 staging, 4 GDI surface (the `D3DKMDT_STANDARDALLOCATION_TYPE` values), 5 user-mode surface, 16 client, 17 context name |
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

Every kind but client and context name is a surface: width, height and pitch
are non-zero. A context name is exactly one page and no surface: the handle an
av context is known by (see Context record), which no device reads.

## Context record (user mode -> guest driver)

The private driver data of one `D3DKMTCreateContextVirtual`. A process that
wants a host av context writes it; the miniport decodes it at `PASSIVE` inside
`DxgkDdiCreateContext` and, like the allocation descriptor, treats every byte as
hostile. It never reaches the device. It names the sub-API, the ring's order,
the reply slot's size, and one allocation of the process's: the page the
context is known by, the one handle a driver can resolve in the process that
made it, so another context of the same process can name this one. The memory
the context runs on is the driver's own (see Context memory). It carries no
doorbell - the miniport owns the doorbell page and rings it itself. 32 bytes.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `AVK` = `0x004B5641` |
| 4 | u32 | sub_api | exactly one of `SubApi` {1 Vulkan, 2 OpenGL, 4 OpenCL, 8 Direct3D} |
| 8 | u8 | ring_order | within the capset's [min, max] |
| 9 | u8[3] | reserved | 0 |
| 12 | u32 | ring | the context's name: an allocation of kind 17, non-zero |
| 16 | u32 | reply_bytes | the reply slot's size, 1 to 16 MiB; the driver rounds it up to pages |
| 20 | u32 | reserved | 0 |
| 24 | u8 | gl_major | OpenGL only, as the setup's options (feature 050); 0 on any other sub-API |
| 25 | u8 | gl_minor | |
| 26 | u8 | gl_profile | |
| 27 | u8 | gl_flags | |
| 28 | u32 | share_context | the name of another av OpenGL context of this process and device, or 0; the one option an OpenCL record may carry (cl_khr_gl_sharing) |

The bytes alone prove magic, one known sub-API, a non-zero name, a reply size
in range, an order in 12..24 and options only the sub-API may carry. The
miniport then resolves the name to a context-name allocation of the calling
process that no other live context uses, and `share_context` - a name, since
dxgkrnl resolves no context handle for a driver - to a live OpenGL context of
the calling process whose name the same device holds, and puts that context's
device id in the setup. Membership in the device's `sub_apis` and the capset's
order range are a second validation against the decoded capset. The record is
unchanged unless the decode returns ok. A record of the kind that named three
allocations under the magic `AVT` is refused whole.

Freeing the name ends the context first: the process is done with it.

## Context memory

A context's ring, fence page and reply slot are pages the miniport allocates
when it makes the context (`MmAllocatePagesForMdlEx`, cached), publishes to the
device as `GUEST` blobs, and maps into the calling process write-back: the ring
(its header, then its data area) and the reply slot writable, and the fence
page read-only. The miniport trusts what the device writes on the fence page,
so the process may read the GPU's progress there but not change it: the view
is mapped from a section handle opened only for reading, which Windows will
not let the process make writable; a copy-on-write view it asks for instead
is a private copy, and changes nothing the driver reads. They are the driver's rather than allocations of the
process's because VidMm evicts a suspended process's allocations - a UWP app in
the background - and maps them back at other pages when it resumes. A device
that had imported the old pages would go on reading the ring and writing
replies into memory the process no longer sees, and into pages Windows had
already handed on. No eviction touches these pages, so a context outlives any
number of suspensions.

The pages go when the context does: CTX_DESTROY, then an unref of each blob,
then the process's views and the pages. Pages a device may still hold - it
did not answer the destroy or the unrefs - are kept out of use for good rather
than freed.

A 32-bit process can address only its low 4 GiB, so a view that would end above
that is refused and the context with it.

### Context views record (user mode -> guest driver, escape)

The private data of one `D3DKMTEscape` whose `hContext` names a live av
context: how the process learns where the miniport mapped its context's ring,
reply slot and, from version 2, fence page. The miniport decodes it at
`PASSIVE`, treats every byte as hostile, and writes its answer back into the
same bytes. 32 bytes.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `AVV` = `0x00565641` |
| 4 | u16 | version | 1 or 2; the answer is of the version asked |
| 6 | u16 | reserved | 0 |
| 8 | u64 | ring | 0 in; the ring's address in the process out |
| 16 | u64 | reply | 0 in; the reply slot's address in the process out |
| 24 | u64 | fence | version 2: 0 in; the fence page's read-only address in the process out. Version 1: reserved, 0 in and out |

An answer has every address of its version non-zero and no two the same; a
process takes nothing else as one. A process asks with version 2 and, refused
with `STATUS_INVALID_PARAMETER` as a driver from before it refuses any record
it does not know, asks again with version 1; it then knows nothing of the
GPU's progress but what its waits tell it.

The fence page's `completed_fence` is how far the host GPU has got: the device
completes a batch's fence only once the GPU has finished what the batch
recorded (for Direct3D 11, `D3d11BatchProgress`), so memory a batch reads may
be reused once `completed_fence` passes the batch's fence. The process reads
that one aligned u64 by itself: the device rewrites the whole page, and a
decode of all of it could mix two writes. An escape whose data is no record the miniport knows answers
`STATUS_NOT_SUPPORTED`; every refusal of this one - a wrong length, version or
reserved field, addresses already filled in, no context named or one that is
not a live av context - is `STATUS_INVALID_PARAMETER` and writes nothing back.

### Next point record (user mode -> guest driver, escape)

The private data of one adapter-level `D3DKMTEscape`. A process names the
present point of its next blt from a host surface, where the present itself
cannot carry the point.

d3d9.dll's `PresentCB` builds the kernel's present itself and drops the
private driver data a `PresentPointRecord` rides in. The Direct3D 9 driver
therefore stages the frame on the host (`PresentPoint9` with a point), then
names that point here before the present. The miniport decodes it at
`PASSIVE` and writes nothing back. 16 bytes.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `AVN` = `0x004E5641` |
| 4 | u32 | point | not 0 |
| 8 | u64 | surface | the shared id the allocation was minted as; not 0 |

The miniport scopes the escape by the calling process (`hKmdProcessHandle`):
it finds an open of the allocation minted as `surface` by a device of that
process, and keeps the point on that open. The next present from the
allocation takes it as it would a `PresentPointRecord` in the present's own
private data. If the present carries its own record, that record wins. The
point is forgotten once that present's last pass has moved, so a later
present naming none is a blt from pages again.

Every refusal is `STATUS_INVALID_PARAMETER`:
- another length or magic, a point or surface of 0;
- no process;
- no open of the surface by a device of the process.

The driver reads its frames back into the allocation then, as it did before.

### Wake event record (user mode -> guest driver, escape)

The private data of one `D3DKMTEscape` whose `hContext` names a live av
context: a process handing the miniport an event of its own to set whenever the
host raises on that context, so that a thread waiting for the host to finish
something sleeps on the event instead of asking on a timer. The miniport
decodes it at `PASSIVE` and writes nothing back. 32 bytes.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `AVE` = `0x00455641` |
| 4 | u16 | version | 1 |
| 6 | u16 | reserved | 0 |
| 8 | u64 | event | the process's handle to an event, with `EVENT_MODIFY_STATE` |
| 16 | u64 | reserved | 0 |
| 24 | u64 | reserved | 0 |

The miniport takes a reference to the event, so the process may close its
handle, and holds it until the context is destroyed. A second record on the
same context replaces the first. Several contexts may name the same event. The
miniport sets it from its interrupt path:

- whenever the context's fence page shows `raised` moved (see Fence page);
- once when the context is lost, because the host faulted it or its fence page
  failed it;
- when the device is reset, and when the context is destroyed.

The last three mean a thread asleep on the event is never left there with
nothing more to come: it wakes, asks, and learns that the context is gone. The
event says only that something happened, never what, and the process asks over
the wire as it would have anyway.

`STATUS_NOT_SUPPORTED` comes from a device without capset bit 6
(`raises`), whose page never moves `raised`, and from a miniport that
predates this record. A process that gets it waits on a timer as before. Every
other refusal is `STATUS_INVALID_PARAMETER`: a wrong length, magic, version or
reserved field, a handle of 0, no context named or one that is not a live av
context, or a handle that is not an event the process may set.

### The submission path

With a context set up, a process submits by writing a batch into the ring the
miniport mapped into it (see Batches), leaving a submission descriptor (AVS) in
the DMA buffer of a `D3DKMTSubmitCommandVirtual`, and letting the miniport ring
the doorbell; the miniport completes the D3DKMT submission fence the process
waits on from the fence-page interrupt (see Fence page). A reply-generating
command's answer is in the reply slot before the fence completes, so a process
that waited reads a finished answer.

### Host window record (user mode -> guest driver, escape)

Host-visible memory is taken on the context by this escape
(docs/host-memory.md): a chunk of the host window, which is host GPU memory of
the kind the record names, and which IMPORT_BLOB then names by its resource.
32 bytes, sent with the av context's handle:

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `AVW` = `0x00575641` |
| 4 | u16 | version | 2 |
| 6 | u16 | operation | 1 open, 2 close |
| 8 | u32 | resource_id | open: 0, answered with the chunk's resource; close: the resource |
| 12 | u32 | memory | open: a `HostMemoryRequest` - bits 0-7 the kind (1 `upload`, 2 `readback`, 3 `vulkan`), bits 8-31 the Vulkan memory type, below 32, for `vulkan` and 0 otherwise; close: 0 |
| 16 | u64 | bytes | open: non-zero, answered with the whole pages granted; close: 0 |
| 24 | u64 | address | 0; an open is answered with the process's view |

An open takes a free host blob id from `0x01000000` to `0x01003FFF`, lowest
first, and the first run of window pages that holds the bytes. The miniport
then sends `RESOURCE_CREATE_BLOB` (`HOST3D`, the context's `ctx_id`, the
request as `blob_id`) and `RESOURCE_MAP_BLOB` at that run, and maps the pages
into the process with the caching the device answered. A close unmaps the view,
sends `RESOURCE_UNMAP_BLOB` and `RESOURCE_UNREF`, and only once the device has
answered frees the pages and the id; pages and an id the device may still hold
are kept out of use for good. A context holds at most 4096 chunks, a quarter of
the ids, and a context that goes closes every chunk it still holds.

Nothing falls back to guest RAM. An adapter with no window, or a device that
refuses the blob or its mapping, answers `STATUS_NOT_SUPPORTED`; every other
refusal is `STATUS_INVALID_PARAMETER`: a malformed record, a context that is
not live, a resource that is none of the context's chunks, every chunk or id
taken, no run of pages that long, or a view the process cannot be given - a
32-bit process's view must end below 4 GiB.

## Present command (guest driver -> device, in a DMA buffer)

What `DxgkDdiPresent` writes at the start of the DMA buffer dxgkrnl gives it,
followed by its sub-rectangles; `DxgkDdiPatch` fills the addresses before the
buffer is submitted. 72 bytes, then 16 per sub-rectangle. A rectangle is
`left, top, right, bottom` as i32, right and bottom exclusive, `left <= right`,
`top <= bottom`.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `AVP` = `0x00505641` |
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
| 0 | u32 | magic | `AVM` = `0x004D5641` |
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

Besides av commands, the driver sends standard virtio-gpu requests (virtio 1.2
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
| `GUEST` (1) | any | at least one, covering `size` | 0 | the guest pages, mapped by the device once at creation and held until unreferenced: a context's ring, fence page and reply slot, a scanout's framebuffer, a present's pages. No guest blob is ever handed to a host GPU |
| `HOST3D` (2) | `MAPPABLE` | none | a `HostMemoryRequest` (see Host window record), in the low 32 bits | host GPU memory the executor library allocates for the context the header's `ctx_id` names (docs/host-memory.md); the guest reaches it through the host-visible shared memory region (id 1, BAR 4) once mapped |

A host blob's id is one of the 16384 from `0x01000000` (`HostBlobId`), a range
above every id the driver gives a guest blob, a scanout or the cursor; its
size is whole 4096-byte pages, and its context must exist. The device asks the
executor library for the memory (`allocate_memory`, keyed by the resource id)
as the blob is created; in renderer mode the renderer's library allocates it
and the dma-buf comes back over the renderer socket (docs/renderer.md).

`RESOURCE_MAP_BLOB` places a `HOST3D` blob at a page-aligned offset of the
region, wholly inside it and overlapping no other mapping, by mapping the
memory's dma-buf there, and answers `VIRTIO_GPU_RESP_OK_MAP_INFO` with the
host's own caching: `VIRTIO_GPU_MAP_CACHE_CACHED` for memory the host maps
write-back, `VIRTIO_GPU_MAP_CACHE_WC` for memory it maps write-combined. The
guest must map the pages the same way. The contents survive
`RESOURCE_UNMAP_BLOB` and a later map elsewhere; unmapped offsets read as
zero. `RESOURCE_UNREF` of a host blob releases it, mapped or not, and is never
refused: the library frees the memory once no import holds it.

| Request | Answer |
|---|---|
| `RESOURCE_CREATE_BLOB` shorter than its 56 bytes, or its entries | `ERR_INVALID_PARAMETER` |
| id 0, or an id a 2D resource or blob holds; a `HOST3D` id outside the host blob range | `ERR_INVALID_RESOURCE_ID` |
| guest entries shorter than `size`; a guest blob with a `blob_id` | `ERR_INVALID_PARAMETER` |
| guest entries that are not guest RAM (the device would see a copy) | `ERR_UNSPEC` |
| a `HOST3D` blob without `MAPPABLE`, with entries, with a `blob_id` that is no request, or with a size of no whole pages | `ERR_INVALID_PARAMETER` |
| a `HOST3D` blob for a context that does not exist | `ERR_INVALID_CONTEXT_ID` |
| guest blob slots exhausted, or memory the library would not allocate | `ERR_OUT_OF_MEMORY` |
| `RESOURCE_MAP_BLOB` of an id that is no host blob (a guest blob included) | `ERR_INVALID_RESOURCE_ID` |
| a blob already mapped, a misaligned offset, one past the region, or an overlap | `ERR_INVALID_PARAMETER` |
| a dma-buf the host could not map there | `ERR_UNSPEC` |
| `RESOURCE_UNMAP_BLOB` of an id that is no host blob / an unmapped blob | `ERR_INVALID_RESOURCE_ID` / `ERR_INVALID_PARAMETER` |
| `RESOURCE_CREATE_2D` over a blob id | `ERR_INVALID_RESOURCE_ID` |
| `RESOURCE_UNREF` of a guest blob a display head is showing | `ERR_INVALID_PARAMETER` |

A device reset releases every display head, then every blob and mapping.

## Blob scanout (guest -> device)

A display head shows a guest blob directly: the host's display shows a
display surface over the device's own mapping of the blob's pages, so no frame
is transferred and no dma-buf is made. The guest may send `SET_SCANOUT_BLOB`
only when the capset carries `feature_dmabuf_scanout`; without it the head
keeps `SET_SCANOUT`, `TRANSFER_TO_HOST_2D` and `RESOURCE_FLUSH` over a 2D
resource, which is what every host started without a GL-capable display
gives.

**`SET_SCANOUT_BLOB`** (96 bytes; virtio 1.2 5.7.6.10) binds head `scanout_id`
to blob `resource_id`, showing the rectangle `r` of a `width` x `height`
framebuffer of `format` at `offsets[0]` with `strides[0]`. Only plane 0 is
carried: a non-zero stride or offset on planes 1 to 3 is refused rather than
dropped, because the display shows one plane. `resource_id` 0 hides the head,
twice in a row without error. A head already showing something is not hidden
first - the new blob is pinned and shown before the old one is let go, so
nothing blank reaches the screen between two pictures.

| Refusal | Answer |
|---|---|
| shorter than 96 bytes, a non-zero `padding`, or a second plane | `ERR_INVALID_PARAMETER` |
| `scanout_id` at or beyond 16 | `ERR_INVALID_PARAMETER` |
| a format outside the eight 32-bit colour formats, a zero `width` or `height`, a stride below `width` x 4 or above `INT32_MAX` | `ERR_INVALID_PARAMETER` |
| a framebuffer whose last row runs past the blob | `ERR_INVALID_PARAMETER` |
| `r` outside the framebuffer, or with a zero side | `ERR_INVALID_PARAMETER` |
| `resource_id` naming no guest blob | `ERR_INVALID_RESOURCE_ID` |
| a head the host does not have, or a blob whose pages the device cannot map as one range | `ERR_UNSPEC` |

The rule behind the table: `ERR_INVALID_PARAMETER` is the guest being wrong and
`ERR_UNSPEC` is the host declining. A refusal changes nothing - the head goes on
showing what it showed, and the new blob is not pinned.

**`RESOURCE_FLUSH`** naming a blob updates every head showing it, clipped to
the part of the head's picture the rectangle covers; a rectangle inside the
framebuffer but outside the picture updates nothing and answers `OK_NODATA`.
`ERR_INVALID_PARAMETER` for a rectangle outside the framebuffer or a blob no
head shows. A flush naming a 2D resource is the base class's, as it always was,
and so is `SET_SCANOUT`: the base class already answers
`ERR_INVALID_RESOURCE_ID` for a resource it does not hold, and a blob is not
one of its resources.

A blob a head is showing is pinned, so `RESOURCE_UNREF` of it is refused until
the head is hidden.

## Fence page (device -> guest)

Offset 0 of the fence resource. The device updates it from its worker thread,
all but the beat word, which its pulse thread writes; the guest reads it from
its interrupt path and from the processes that wait on it. 48 bytes.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `AVF` = `0x00465641` |
| 4 | u32 | flags | bit 0 `faulted`; no other bit |
| 8 | u64 | completed_fence | highest fence id whose work finished |
| 16 | u64 | progress | increments whenever the device makes progress (TDR heartbeat) |
| 24 | u32 | fault_code | 0 unless `faulted` |
| 28 | u32 | reserved | 0 |
| 32 | u64 | raised | out-of-band raises delivered (feature 052); capset bit 6 |
| 40 | u64 | beat | moves while the device is at work for the guest (below) |

The fault codes, which say where to look rather than only that something
failed: 1 ring header, 2 ring tail, 3 batch header, 4 batch order, 5 command
framing, 6 undecodable, 7 device lost, 8 no executor, 9 broken executor. The
last is a library answering a value the executor ABI does not define, and it is
separate from device lost on purpose - folded together, a bug in an executor
reads as a GPU fault and is chased in the wrong place.

Against what the guest has seen: `completed_fence`, `progress`, `raised` and
`beat` never decrease, `completed_fence` never exceeds the highest fence id submitted, and
once `faulted` is observed it stays set for the context's lifetime.

**When the device writes it.** At the context setup, before the ring is
attached: magic, flags 0, completed 0, progress 0, fault code 0, beat 0. Then from the
context's worker: after every command it executes, progress moves on (a
heartbeat: a long batch reads as busy, not hung); after every batch it
completes, completed is that batch's fence and progress moves on again; and
once, if the context faults, the page reads `faulted` with the last completed
fence, the progress so far and the fault code. Nothing is written after a
fault. Every write is the whole page but the beat from the device's own counters; the
device never reads the page back, so a page the guest overwrites is rewritten
by the next write.

**The beat.** A worker writes the page only between commands, and one command
can sit in a host call for seconds: a host driver holds the call until the GPU
drains a queue (a Radeon 610M running a few hundred compute dispatches a
frame), and nothing else on the page moves meanwhile. The device's pulse - one
thread per device (`DevicePulse`) - moves `beat` on every bound context's page
every 250 ms while any context of the device is at work: running a batch, a
host call included, or keeping watch on GPU results it has yet to publish.
Every page, not only the busy context's, because a context whose work waits
behind another's is waiting on the same device. With nothing at work nothing
beats, and the pulse sleeps until a worker goes to work. The worker never
writes the word - its whole-page writes stop at offset 40 - so the two writers
never store each other's words, and the beat only goes forward; a faulted
page is beaten no more (at most once after the fault, by a pulse already
inside its pass).

**How long a guest waits.** Never for a fixed time. A process waiting on its
context (`KmtTransport`, through `HostWatch`) gives up only when the page says
`faulted`, or when nothing the device publishes - the ring's head,
`completed_fence`, `progress`, `beat` - has moved for 30 s, which no device at
work is ever quiet for. A host GPU that is truly hung is the host kernel's to
reset, and the reset reaches the guest as a fault.

**The out-of-band raise** (feature 052). A host API that finishes work on a
thread the device never gave it - OpenCL's event callbacks - reaches the guest
through this page too, and the shape it is allowed is exactly what the guest's
history check already accepts: **`progress` moves, `completed_fence` does not,
and the interrupt is raised**, and `raised` moves with it. Nothing completed,
because no batch did. The
foreign thread only sets a flag and wakes the context's worker, the way the
doorbell's ioeventfd does; the worker writes the page. Raises coalesce, so a
burst of host completions is one move of `progress` and one interrupt, and the
page never says which event finished - there is no room for one and no need,
since the guest asks over the wire. A faulted context raises nothing.

`raised` counts the raises alone, apart from `progress`, which every command
moves too. A device announcing capset bit 6 (`raises`) keeps it;
one that does not leaves it 0. The driver uses it to wake a process that
registered a wake event on its context (the `AVW` escape, below), so that
process sleeps until the host finishes something instead of asking on a timer.

| Fault code | Reason |
|--:|---|
| 1 | ring header malformed, or its order changed |
| 2 | ring tail moved backwards, or more than the ring ahead of head |
| 3 | batch header malformed, or the batch cannot fit the ring |
| 4 | batch fence not greater than the last |
| 5 | commands framed wrong, a transport command with a body, or an answer asked for with no reply slot |
| 6 | a payload the executor cannot decode |
| 7 | the host device was lost |
| 8 | the context's executor could not be created |

A faulted context's executor is destroyed at once, on its worker; the context,
its doorbell and its blobs stay until `CTX_DESTROY` or a reset.

A guest can fault its own context on purpose, and the Vulkan ICD does so for
its probe: `vkAvForceContextFault(VkPhysicalDevice)`, an ICD-local
physical-device-level entry and not an extension, sends a `vkCreateInstance`
frame cut after its structure type on the instance's own transport, which the
executor cannot decode (fault 6). No other context is touched.

**Recovery.** The guest's engine reset destroys the context (`CTX_DESTROY`),
writes a fresh ring header, and creates and sets up a context again - the same
id, blobs and doorbell may be reused - which starts healthy from progress 0.
An adapter reset is a virtio reset: every context is destroyed and a new
driver starts from nothing. A context the guest wants to stop without a fault
(preemption) needs no command: the guest stops publishing its tail.

## The driver's sequence

The order a guest driver follows (`src/miniport/av_context/`), checked end to end against
the device core by the loopback suite (`tests/loopback/`):

1. **Capset.** `GET_CAPSET_INFO` for index 0, then `GET_CAPSET` for the id and
   version it names. A capset that does not decode, is not `0x72`, or is of
   another major ends the query; nothing below starts without one.
2. **Memory.** A ring of `32 + 2^order` bytes rounded up to pages with a fresh
   header, a fence page and a reply slot, zeroed pages the driver owns (see
   Context memory).
3. **Setup.** `CTX_CREATE` with capset `0x72`, then `RESOURCE_CREATE_BLOB`
   (guest memory, the pages as entries) for the ring, the fence page and the
   reply slot,
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
   `RESOURCE_UNREF` for each blob created, in order; then the memory is freed,
   or kept out of use for good if the device did not answer. After a virtio
   reset the driver abandons its contexts - the device holds none - and frees
   their memory without requests.

## Fence queues and interrupts (device -> guest)

`virtio-gpu-av-pci` has 68 virtqueues: control (0), cursor (1), a fence
queue per doorbell page, queue `2 + n` for doorbell `n`, the display line
(66, see "Display clock") and the present line (67, see "Present engine"). A fence queue carries
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

The driver MUST write every fence queue's `queue_msix_vector` before `DRIVER_OK`.
The device has no `guest_notifier_mask`, so QEMU attaches a queue's irqfd only
when its vector is set before `DRIVER_OK` or when that MSI-X entry is unmasked; a
vector written later, to an entry the guest already unmasked, gets a route and
no irqfd, and every interrupt on it is lost (`hw/virtio/virtio-pci.c`,
`kvm_virtio_pci_vector_use_one`). `avgpu.sys` assigns them all at start and
writes the same value again when a context is set up. Feature 044 found this
live: each packet after a context's first completed only when an unrelated DPC
served it, 2-3 s later, past TdrDelay under load; qtest has no irqfd and could
not show it.

## Display clock (device -> guest)

The display's vertical blank is the device's. A guest timer cannot be one: a
DPC that runs late restarts its schedule and the blanks it missed are gone, and
whatever the guest reports, the frame the host shows is decided by when the
host's window draws. So the device keeps one clock per head, at the head's
refresh rate, on the host's `CLOCK_MONOTONIC` and against absolute deadlines:
a late wake-up still counts every blank that passed. At each blank it latches
the newest frame the guest has posted for the head, shows exactly that frame,
and tells the guest which one it was. Everything in the guest that paces to
the display - dxgkrnl's vsync, the scan line, the user-mode drivers' pacers -
reads this one timeline.

### The display line

Queue 66, after the fence queues, carries no requests and exists for its
interrupt, as they do: the driver gives it a vector with `queue_msix_vector`
before `DRIVER_OK` (`avgpu.sys` treats it as fence queue 64, so it shares a
fence vector). The device raises it on the blanks the guest asked for (the
page's `interrupt_every`), after the page says what the blank was. One
interrupt may stand for several blanks - the guest reads the count, never
counts interrupts.

### Registering the page (control queue)

| Offset | Size | Field | |
|--:|--:|---|---|
| 0 | 24 | header | a `virtio_gpu_ctrl_hdr`, type `0x02F4`, context 0, no fence |
| 24 | 8 | address | guest physical address of a 4 KiB page, 4 KiB aligned; 0 stops publishing |

The device answers `OK_NODATA` and from then on writes the page below, or
`ERR_INVALID_PARAMETER` for an address that is not aligned or not guest RAM it
can map, leaving the page it had. A virtio reset forgets the page; the driver
registers it again at each start.

### A head's rate (control queue)

| Offset | Size | Field | |
|--:|--:|---|---|
| 0 | 24 | header | a `virtio_gpu_ctrl_hdr`, type `0x02F5`, context 0, no fence |
| 24 | 4 | head | below the device's `num_scanouts` |
| 28 | 4 | refresh | millihertz, 1000 to 1000000 (1 Hz to 1 kHz) |

`OK_NODATA`, or `ERR_INVALID_PARAMETER` for a head or a rate out of range. The
head's next blank comes one new period after the blank before it, and the
sequence carries on: a rate change is never a lost or repeated blank. A head
runs at 60 Hz until told otherwise.

### Posting a frame

`SCANOUT_IMAGE` (`0x02F0`, resource id, then a u32, then the u64 key) names
the head's next frame. Its u32 is the frame: nonzero, the device shows the key
at the head's next blank, and a later post before that blank replaces it; 0,
at once, as a device from before this section did. Frame numbers are the
guest's, counted per head from 1, skipping 0 when they wrap; the device only
compares them for equality.

### The page, 4 KiB, little-endian

| Offset | Size | Writer | Field | |
|--:|--:|---|---|---|
| 0 | 4 | device | magic | `0x00435641` (`"AVC"`) |
| 4 | 4 | device | version | 1 |
| 8 | 4 | device | heads | head records that follow, 1 to 16 |
| 12 | 20 | | reserved | 0 |
| 32 + 64 h | 64 | | head `h` | below |

Each head's record:

| Offset | Size | Writer | Field | |
|--:|--:|---|---|---|
| 0 | 4 | device | generation | odd while the device writes the record, even once it is whole |
| 4 | 4 | device | latched | the frame the blank at `sequence` put on screen; 0 before the first |
| 8 | 8 | device | sequence | blanks since the head's clock started, the first 1 |
| 16 | 8 | device | vblank | the host's `CLOCK_MONOTONIC` of that blank, in ns |
| 24 | 4 | device | period | ns between blanks now: 10^12 / `refresh`, rounded down |
| 28 | 4 | device | refresh | millihertz now |
| 32 | 4 | guest | interrupt_every | raise the display line on each blank whose sequence is a multiple of this; 0 raises nothing |
| 36 | 28 | | reserved | 0 |

A reader takes `generation`, the fields, then `generation` again, and trusts
them only when both were the same even number; it tries again otherwise, a
bounded number of times. The device writes nothing else in the page, and the
guest writes only `interrupt_every`; the device reads it at each blank and
treats it as hostile (any value is a divisor or none). The guest never trusts
the device either: a wrong magic, version or head count is a page it ignores,
and a sequence that goes backwards is a blank it does not report.

## Scanout planes (guest -> device)

A device that offers `feature_scanout_planes` (capset bit 7, 128) composes a
head's overlay planes, as a display controller layers them at scanout: avgpu
then offers Windows four planes a source instead of one (`OverlaySupport`), and
carries the planes each flip left before the frame that shows them.

`SCANOUT_PLANE` (`0x02F8`, 88 bytes) is one layer of one head:

| Offset | Size | Field | |
|--:|--:|---|---|
| 0 | 24 | header | type `0x02F8` |
| 24 | 4 | head | the scanout the plane is on |
| 28 | 4 | layer | 0 to 3, 0 the bottom |
| 32 | 4 | flags | bit 0 shown, bit 1 filtered (scaled with a filter, not the nearest pixel) |
| 36 | 4 | reserved | 0 |
| 40 | 8 | key | the host image the plane shows (§3.1 of d3d-desktop.md); 0 for a plane not shown |
| 48 | 16 | source | i32 left, top, right, bottom in the image |
| 64 | 16 | destination | i32 left, top, right, bottom on the head |
| 80 | 4 | head width | the head's size, which the device composes at |
| 84 | 4 | head height | |

The guest folds the plane's clip and the head's edges into both rectangles
before it sends them, cutting the source in the same proportion, so the device
draws exactly the source into exactly the destination. A plane with nothing
left on the head is sent not shown. The guest sends a layer only when what it
shows changed, and every layer of a source flipped any way but the MPO3 DDI is
sent not shown. A shown plane with no key, an empty or inverted rectangle or a
source left of or above its image is refused, as is a layer past 3, a head of
no size or one wider or taller than 16384.

The device keeps each head's layers. A head that shows nothing but its base
plane filling it is shown as before planes, from `SCANOUT_IMAGE`'s key; any
other is composed: the head's planes, by layer, drawn on the host GPU into one
image of the head's size, black where no plane is (`PlaneComposer`, through
the executor's `compose_planes`), at the blank that latches the frame
`SCANOUT_IMAGE` posts after them. The export socket's head request
(external-export.md §4) hands the same composition to a consumer.

## Present engine (guest -> device, device -> guest)

A present's pixels - dxgkrnl's own blts and fills, and an application's
blt-model present into its window's redirection surface - are the device's
work, and the device does it on a thread of its own. The present copy above
answers only once every pixel has moved, from QEMU's main loop: the
application's present waited for it, and for whatever else the main loop was
doing (drawing the host's window). Here the miniport hands a present over and
returns; the device moves the pixels off the main loop, in the order it was
given them, and says which it has moved through a page and an interrupt line,
as the display clock says its blanks. dxgkrnl's packet for the present
completes then, and nothing reads the destination before.

A device offers the engine with capset bit 4, `present_engine`, and a source
that is a host image rather than guest pages with bit 5, `present_from_image`;
the miniport uses the present copy above on a device without them.

### The present line

Queue 67, after the display line, carries no requests and exists for its
interrupt, as the fence queues do (`avgpu.sys` treats it as fence queue 65).
The device raises it after writing `completed` whenever tickets complete; one
interrupt may stand for several.

### Registering the page (control queue)

`0x02F6`, laid out as the display clock's registration: the header, then the
guest physical address of a 4 KiB page, 4 KiB aligned, 0 to stop. `OK_NODATA`,
or `ERR_INVALID_PARAMETER` for an address that is not aligned or not guest RAM
the device can map. A virtio reset forgets the page and every present not yet
moved; the miniport registers the page again at each start.

### Queueing a present (control queue)

| Offset | Size | Field | |
|--:|--:|---|---|
| 0 | 24 | header | a `virtio_gpu_ctrl_hdr`, type `0x02F7`, context 0, no fence |
| 24 | 8 | ticket | the count of queued presents since the page was registered, this one included: the first is 1 |
| 32 | 4 | source_kind | 0 guest pages, 1 a host image |
| 36 | 4 | source_point | an image source's: the present of the surface whose frame is read, counted from 1 by the surface's writer; otherwise 0 |
| 40 | 8 | source_surface | an image source's: the shared surface's id, as the miniport minted it (`SharedObjectRecord`); otherwise 0 |
| 48 | 4 | operation | 1 blt, 2 colour fill |
| 52 | 4 | flags | bit 0 swaps each pixel's first and third byte, as in the present copy; blt only |
| 56 | 4 | colour | a fill's four bytes; 0 for a blt |
| 60 | 4 | region_count | 1 to 256 |
| 64 | 4 | source_entries | a page source's: 1 to 16384; an image's and a fill's: 0 |
| 68 | 4 | destination_entries | 1 to 16384 |
| 72 | 4 | source_pitch | a page source's, as in the present copy; an image's and a fill's: 0 |
| 76 | 4 | destination_pitch | as in the present copy |
| 80 | 16 each | source entries | virtio memory entries |
| | 16 each | destination entries | |
| | 24 each | regions | as in the present copy; an image source's x and y are the image's pixels |

It travels as the present copy does: the fixed part in a control slot, the rest
as a second read-only buffer of the miniport's contiguous memory, which the
device reads when it takes the request and the miniport keeps until the ticket
completes. The device answers at once: `OK_NODATA` once it has taken the
request - the pixels move later - or `ERR_INVALID_PARAMETER` for one that
breaks a rule above or names memory that is not guest RAM, which moves nothing.
Either way the ticket completes in its turn: the device counts every queued
present it is handed while a page is registered, refused ones included, and
the nth completes as ticket n - a request whose ticket is not its count is
refused. `completed` never passes a ticket the device has not finished with,
and never skips one. Without a page the request is refused and not counted.
The device holds at most 16 presents it has not moved; one past that is
refused, so a miniport keeps no more than 16 outstanding.

An image source is a frame of a shared surface (docs/shared-surfaces.md):
the surface's contents as its writer presented them for the `source_point`th
time, in the order of the writer's own commands. The Direct3D drivers number
each present of a shared surface - `PresentPoint` (`0x040d`) for Direct3D 11,
the `point` of `PresentResource12` for Direct3D 12 - and hand the point to the
kernel in the present's private driver data (`PresentPointRecord`, 16 bytes:
u32 magic `0x00505641`, u32 point, u64 reserved 0); the miniport adds the
source allocation's shared id, which it minted, so no process names a surface
it was not given. The device stages a surface's frames only once a blt has
asked for one of them - a flip-model buffer the compositor reads itself costs
nothing - so the first blt of a surface is refused, and each one after reads a
frame copied where the writer presented it: what the writer draws after the
present never reaches the destination. The device waits for the writer to
reach the point and the GPU to copy it, up to half a second - past that the
present is refused - and lets the frame go once read, or refused. It arrives
in the surface's own byte order (a red-first surface arrives red-first, as its
guest pages would hold it; the swap flag says whether the destination wants
the other order); `x` and `y` of a region are the surface's pixels, and a
region with a row outside the frame or the destination is refused where it
goes wrong, as in the present copy. A device offers image sources only where
it reads the frames itself: one whose contexts run in a renderer process does
not set bit 5.

A Direct3D 9 fullscreen primary is flipped, not presented as a blt: its
driver sends `PresentPoint9` (`0x210a`) with point 0 as the primary is made
and before every flip, and the host copies the surface into a Direct3D 11
texture it shares under the primary's key, where DWM opens the primary and the
scanout exports it like any other keyed surface. Direct3D 9's host device is
not Direct3D 11's, so the copy goes through the CPU.

### The page, 4 KiB, little-endian

| Offset | Size | Writer | Field | |
|--:|--:|---|---|---|
| 0 | 4 | device | magic | `0x00505641` (`"AVP"`) |
| 4 | 4 | device | version | 1 |
| 8 | 8 | device | completed | the last ticket whose pixels have moved, or that was refused; 0 before the first |
| 16 | 8 | device | refused | tickets refused so far |
| 24 | 4072 | | reserved | 0 |

`completed` and `refused` are each written as one aligned eight-byte store, and
`completed` before `refused`: a guest that reads `refused` and then `completed`
never reads more refused than completed. The guest trusts nothing: a wrong magic or
version is a page it ignores, and a `completed` that goes backwards or past the
last ticket it sent is one it does not act on.

## Wire recordings (device -> a file on the host)

A device started with `record=<path>` writes down what crossed the wire
(feature 065): a versioned file header, then length-prefixed typed records, in
the order the device took them, flushed as each is written so a QEMU that dies
inside a host driver leaves the batch it was running. Recording is off by
default and nothing else on the submission path changes when it is.

This is not part of the guest-visible protocol - no guest ever reads one - but
it is the wire written down, so it is specified here beside what it records.
Every convention above holds: little-endian, fixed-size structures, a 32-bit
ASCII magic, reserved fields zero and rejected when not, and a reader that
trusts nothing it did not write.

### File header, 64 bytes, at offset 0

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `AVR` = `0x00525641` |
| 4 | u16 | major | 1; a reader refuses another |
| 6 | u16 | minor | 0; a higher one is read as far as the reader knows |
| 8 | u16 | wire_major | the protocol version the recorded device spoke |
| 10 | u16 | wire_minor | |
| 12 | u32 | kinds | bit `kind - 1` for every kind this recording keeps |
| 16 | u64 | started | the host's monotonic nanoseconds when the file was opened |
| 24 | u64 | limit | the ceiling in bytes, 0 for none, otherwise >= 64 |
| 32 | u32 | wraps | times the recording wrapped back to this header |
| 36 | u32 | vendor_id | the bound host GPU's `VkPhysicalDeviceProperties::vendorID` |
| 40 | u32 | device_id | `::deviceID` |
| 44 | u32 | reserved | 0 |
| 48 | u8[16] | gpu_uuid | `VkPhysicalDeviceIDProperties::deviceUUID`, zeroes when unbound |

At the ceiling the recording either stops (the default) or wraps: the file is
truncated back to this header, `wraps` is raised, and writing continues, so the
newest records survive and the file never passes the ceiling. Either way the
device log says so once.

### Record header, 32 bytes, in front of every record

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `AVD` = `0x00445641` |
| 4 | u16 | kind | 1..7 below; a reader stops at any other |
| 6 | u16 | reserved | 0 |
| 8 | u32 | context | the device's context id, 0 when the record names none |
| 12 | u32 | length | payload bytes that follow, at most 16 MiB |
| 16 | u64 | timestamp | nanoseconds since the file header's `started` |
| 24 | u64 | fence | the batch this record belongs to, 0 when it belongs to none |

A record occupies `32 + length` bytes and is not padded.

### Record kinds

| Kind | Name | Payload | Written when |
|--:|---|---|---|
| 1 | context setup | the context setup above, 32 or 48 bytes | a context binds |
| 2 | batch | the batch's command bytes, exactly as staged for the executor | before the batch's first command runs |
| 3 | reply | the answer bytes, 0 or more | the answer is written to the reply slot |
| 4 | fence completed | empty | the fence page records the batch complete |
| 5 | fault | 8 bytes: u32 fault code, u32 reserved | a batch fails |
| 6 | blob | 32 bytes: u32 action (0 created, 1 imported, 2 released), u32 reserved 0, u64 id, u64 offset 0, u64 size | a blob is created (the resource and its bytes), imported into a context (the import's id, with the resource it imported in `size`), or released (the resource, or in a context's record the import's id; `size` 0) |
| 7 | blob bytes | 16 bytes: u64 blob_id, u64 offset; then the snapshot | never written now: an import is host GPU memory the device keeps no copy of (docs/host-memory.md), so a replay reports a context's first import as unreplayable and gives that context up |

A batch's payload is the same sequence of framed commands *Execution* above
describes, so a reader walks it with the same header. A recording holds what the
executor library received, which is the guest's bytes after the device
validated the framing around them and nothing more: the bytes inside a command
are still the guest's, and every tool that reads a recording treats them as
hostile.
