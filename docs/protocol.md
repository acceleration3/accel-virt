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
virtio-gpu-rvk-pci presents that function's vendor, device and revision as its
own (`src/device/gpu/host_pci_identity.hpp`); the subsystem stays `1af4:5256`,
which is how the driver package and the guest's programs recognise the device.
A GPU with no PCI function (llvmpipe) leaves virtio-gpu's `1af4:1050`.

Each feature bit is something the host **proved**, not something it hopes for
(`src/device/probe/host_feasibility.cpp`):

| Bit | Name | What the device had to show |
|---|---|---|
| 0 | `host_visible_memory` | the device has a host-visible region at all: `hostmem=` is non-zero, so there is a BAR to place blobs in |
| 1 | `gpu_exported_memory` | memory allocated on the bound host GPU can be placed in that region and read back correctly through it (feature 039's `HostMemoryProbe`; on NVIDIA this works through dma-buf and not through an opaque fd) |
| 2 | `guest_memory_import` | the host GPU can address guest pages directly, `VK_EXT_external_memory_host` (feature 039's `GuestImportProbe`, which refuses anything that is not a direct mapping) |
| 3 | `dmabuf_scanout` | a display head's console can show a dma-buf, so a blob may be scanned out instead of transferred (feature 058) |

Bit 3 is the one bit the device cannot decide when it describes the capset, and
the reason is QEMU's own order: `qemu_init_displays()` runs after
`qmp_x_exit_preconfig()` has realized every device, and it is the display
backend - not the device - that gives a console its GL context. So it is
decided when the guest first reads the capset, once, and the answer the guest
was given is the one that stands for the life of the device.

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
32 bytes at minor 0; 48 at minor 1 and later, and a setup whose length is not
its minor's is malformed.

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
| 32 | u8 | gl_major | minor 1: 1..4 for OpenGL; 0 means the host's default context, and then every option below is 0 |
| 33 | u8 | gl_minor | at most `gl_major`'s highest minor (1.5, 2.1, 3.3, 4.6) |
| 34 | u8 | gl_profile | 0 none, 1 core, 2 compatibility |
| 35 | u8 | gl_flags | bit 0 debug, 1 forward compatible, 2 robust access, 3 lose context on reset; 4-7 zero |
| 36 | u32 | share_context | the device context id of a bound OpenGL context to share objects with, or 0 |
| 40 | u8[8] | reserved | 0 |

Options belong to OpenGL: a setup of another sub-API carries all eight option
bytes zero. The device refuses (`ERR_INVALID_PARAMETER`) a share context that
is not another bound OpenGL context of the device, and non-default options when
its executor library is older than version 4.

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
| a setup naming a ring, fence or reply resource that is not a blob | `ERR_INVALID_RESOURCE_ID` |
| `size` not 32, the buffer short or malformed, a setup the capset does not allow, a blob too small for its role, a ring header that is not fresh, a doorbell another context holds, or a context already set up | `ERR_INVALID_PARAMETER` |
| `RESOURCE_UNREF` of a blob a context is bound to | `ERR_INVALID_PARAMETER`, the blob kept |
| `RESOURCE_UNREF` of a blob an IMPORT_BLOB still holds | `ERR_INVALID_PARAMETER`, the blob kept |

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
| `0x00000002` | IMPORT_BLOB | 32 bytes: u32 resource_id, u32 reserved, u64 offset, u64 size, u32 memory_type, u32 reserved | 16 bytes: u32 status, u32 reserved, u64 device_memory |
| `0x00000003` | RELEASE_BLOB | 8 bytes: u64 device_memory | empty |
| `0x00000004` | SHARE_OBJECT | 32 bytes: u64 object_id, u32 kind, u32 reserved, u64 wire_handle, u64 size | 16 bytes: u32 status, u32 reserved, u64 zero |
| `0x00000005` | OPEN_SHARED_OBJECT | the same 32 bytes, wire_handle 0 | 16 bytes: u32 status, u32 reserved, u64 wire_handle |
| `0x00000006` | RELEASE_SHARED_OBJECT | the same 32 bytes, wire_handle 0 | 16 bytes: u32 status, u32 reserved, u64 zero |

A NOP does nothing but complete, which makes it the round trip a guest times to
measure the transport alone rather than a backend (feature 044). It moves the
context's progress like any command run.

IMPORT_BLOB hands `size` bytes at `offset` of one of the context's guest blobs
to its executor as memory of type `memory_type`, and answers the wire handle -
for Vulkan the `VkDeviceMemory` - the guest names that memory by from then on
(feature 045). A context's resources are laid out from its ring: the ring, the
fence page and the reply slot, then 64 chunk slots, so the resource must be one
of the ids `ring_resource + 3` to `ring_resource + 66`, and a `GUEST` blob.
The device maps the blob's scattered pages again, in order, as one host range
(the pages themselves, not a copy), and the executor imports `offset` bytes into
that range as a host pointer; one range serves every import of the blob and
goes with the last of them. While any import holds a blob, `RESOURCE_UNREF` of
it answers `ERR_INVALID_PARAMETER`, so its id is never re-created over other
pages under a live import.

An OpenGL context imports once (feature 050): its guest ICD binds one
host-visible chunk as its blob arena and imports all of it at offset 0 with
memory type 0, and the OpenGL backend adopts that host range as the context's
region - bulk data a GL command names by offset into the arena (a
`glReadPixels` destination, a texture upload, a buffer map's copy) is read and
written there in place. A second import into the same context is refused; the
wire handle it answers is the region's address, which no GL command names.

A body of any length but its command's is command framing, fault 5, as for a
NOP with a body. Every other refusal is an answer, and the context runs on:

| status | IMPORT_BLOB refused because |
|--:|---|
| 0 | not refused: `device_memory` names the import, never 0 |
| 1 | a reserved field is not 0, `offset` or `size` is not a whole number of 4096-byte pages, `size` is 0, or `offset + size` passes 2^64 |
| 2 | the resource is not a `GUEST` blob in one of the context's chunk slots |
| 3 | `offset + size` is past the blob |
| 4 | the host cannot map the blob's pages as one range (RAM with no file behind it, pages that are not whole, more than 16384 runs) |
| 5 | the context has no executor, its library is older than ABI version 3, or the library refused the import |
| 6 | the context holds 256 imports, or the host has no memory for the table of them |

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

**SHARE_OBJECT** publishes an object this context's guest already made. The
device asks the context's executor for a descriptor of `wire_handle`, records
it under `object_id` for the whole device, and counts this context as its first
holder. The same context publishing the same handle under the same id again
answers ok and changes nothing.

**OPEN_SHARED_OBJECT** hands the context's executor that descriptor and answers
the handle its executor made of it - a `VkDeviceMemory`, a GL memory object
name - which the guest names in that API's own commands from then on. A context
opening an id it already holds is refused: a second handle for one object in
one context is one the guest could free twice.

**RELEASE_SHARED_OBJECT** destroys what this context made of the object and
drops its reference. An id this context never held answers ok and does nothing,
as RELEASE_BLOB does. A context destroyed releases everything it still holds,
and the device's last holder dropping closes the descriptor - so an object
outlives the context that published it, which is the whole point.

Only Vulkan exports a descriptor; OpenGL imports one and exports none, and
OpenCL does neither yet. A guest asking an executor for what its API cannot do
is answered status 4.

| status | a shared-object command refused because |
|--:|---|
| 0 | not refused: an OPEN answers a non-zero `wire_handle`, the other two answer 0 |
| 1 | `object_id` is 0, `kind` is not one of the three, a reserved field is not 0, `size` is not what its kind has, or a `wire_handle` travelled with a command that allows none |
| 2 | SHARE: a wire handle this context's executor never made. OPEN and RELEASE: an object this device does not hold |
| 3 | the kind or the size disagrees with the object the device holds |
| 4 | the context has no executor, its library is older than ABI version 6, or the library refused the export or the import |
| 5 | the device holds 256 objects, or this context holds 64, or the host has no memory for one more |
| 6 | SHARE: this id is already published with a different descriptor. OPEN: this context already holds it |

A refused command answers `wire_handle` 0, and a guest that sees one on a
refusal must not believe it: the answer's own decode refuses that shape.

#### Allocation private driver data

Private data of exactly 48 bytes is an allocation that is not shared, which is
every allocation before feature 053. Private data of 88 bytes is an
`AllocationDescriptor` followed by a 40-byte `SharedObjectRecord`, which is an
allocation asking for an identity: `DxgkDdiCreateAllocation` mints the id and
the adapter's own number into the record it was handed and writes both back, so
the value the runtime keeps is the one the opening process is given. Any other
length is refused. At `DxgkDdiOpenAllocation` the miniport believes nothing it
did not write: an id it never minted, an id another adapter minted, a record
that does not decode, and a kind or a size that disagrees with the object are
each refused with nothing opened.

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


`src/device/abi/rvk_executor.h`, now version 5, is what runs a context's commands on
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

**Version 3 adds `imported_handle(executor, blob_id)`**, because `import_blob`
answers only whether it imported and IMPORT_BLOB's guest needs the handle the
import became. The device chooses every `blob_id` - a per-context count, never
reused - calls `import_blob` with an `RVK_BLOB_HOST_POINTER` handle whose
`reserved` field carries the memory type the guest asked for, then asks
`imported_handle` for the wire handle; 0 means the import made nothing a guest
can name, and the device releases it and answers status 5. Two imports
answering to one handle are refused the same way, since RELEASE_BLOB of one
would release both. A version 2 library still loads and still imports for the
capset's purposes, but the device never asks it for an IMPORT_BLOB; a table
claiming version 3 without `imported_handle` fails realize.

**Version 4 adds `create_with_options(sub_api, gpu_uuid, options, share)`**
(feature 050): `create` with an OpenGL context's version, profile and flags
(`RvkContextOptions`, null for the defaults) and the executor of the context it
shares objects with (null for none), which the device resolves from the setup's
`share_context` among the executors its contexts published, on the same GPU, and
keeps alive until the create returns. Other sub-APIs refuse options and shares;
a version the host refuses is a null executor, which the guest sees as the
context's fault. A version 3 library is only handed the defaults; a table
claiming version 4 without `create_with_options` fails realize. On OpenGL,
`import_kinds` answers `RVK_BLOB_HOST_POINTER` and `import_blob` takes the
pointer as the context's bulk-data region - one at a time, its handle the
pointer's address - rather than as a GL object.

**Version 5 adds `set_signal(executor, signal, context)`** (feature 052), the
first entry in the table that runs the other way: the device hands the library
a function and an opaque, and the library calls `signal(context)` when the host
API finishes something on a thread the device never gave it - the host's OpenCL
driver calling back an event callback. Nothing about what completed crosses
here; the call must do almost nothing and must not call back into the table,
and the guest asks. The device gives it on the context's worker as soon as
there is an executor to give it to, and never takes it back; a table claiming
version 5 without `set_signal` fails realize, and a version 4 library signals
nothing, so a guest on one waits for its events at a synchronising call
instead. Because OpenCL cannot unregister a callback, a signal may arrive after
the executor it names is destroyed: the `context` the device hands over lives
as long as the device does, and a signal for a context that is gone is
tolerated rather than a fault.

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

**What the guest is told is host visible** is what IMPORT_BLOB can serve. The
executor answers `vkGetPhysicalDeviceMemoryProperties` (all three spellings)
with `HOST_VISIBLE`, and `HOST_COHERENT` and `HOST_CACHED` with it, cleared from
every type the driver will not import a host pointer into - NVIDIA's BAR type,
here - since the guest maps host-visible memory as its own RAM and has no other
way to reach it. The mask is learnt once per physical device, from a device the
executor makes for the question over an anonymous page; `import_blob` still
checks the type against the real pointer. Buffers and images made on the device
imports go to carry `HOST_ALLOCATION` external-memory create info where the
driver reports the create importable, unless the guest's chain names its own.

## Ring header (shared, both write)

Offset 0 of the ring resource; the ring's data area follows at offset 32 and
holds `2^order` bytes. 32 bytes.

| Offset | Type | Field | Writer | Rule |
|--:|---|---|---|---|
| 0 | u32 | magic | guest, once | `RVKR` = `0x524B5652` |
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

## Context record (user mode -> guest driver)

The private driver data of one `D3DKMTCreateContextVirtual`. A process that
wants a host rvk context writes it; the miniport decodes it at `PASSIVE` inside
`DxgkDdiCreateContext` and, like the allocation descriptor, treats every byte as
hostile. It never reaches the device: it names, by the process's own D3DKMT
allocation handles, the ring, fence page and reply slot the miniport must
publish as guest blobs before it sends the context's setup. It carries no
doorbell - the miniport owns the doorbell page and rings it itself. 32 bytes.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `RVKT` = `0x544B5652` |
| 4 | u32 | sub_api | exactly one of `SubApi` {1 Vulkan, 2 OpenGL, 4 OpenCL} |
| 8 | u8 | ring_order | within the capset's [min, max] |
| 9 | u8[3] | reserved | 0 |
| 12 | u32 | ring | the ring allocation, non-zero |
| 16 | u32 | fence | the fence-page allocation, non-zero, different from `ring` |
| 20 | u32 | reply | 0 for no reply slot; otherwise different from `ring` and `fence` |
| 24 | u8 | gl_major | OpenGL only, as the setup's options (feature 050); 0 on any other sub-API |
| 25 | u8 | gl_minor | |
| 26 | u8 | gl_profile | |
| 27 | u8 | gl_flags | |
| 28 | u32 | share_context | the ring allocation handle of another rvk OpenGL context of this process and device, or 0 |

All eight option bytes zero is the record of old. The bytes alone prove magic,
one known sub-API, non-zero distinct ids, an order in 12..24 and options only
an OpenGL record may carry; the miniport resolves `share_context` - a ring
handle, since dxgkrnl resolves no context handle for a driver - to a live
OpenGL context of the calling process whose ring the same device holds, and puts
that context's device id in the setup; membership in the device's `sub_apis` and the capset's order
range are a second validation against the decoded capset. The record is
unchanged unless the decode returns ok.

### The submission path

With a context set up, a process submits by writing a batch into the ring it
mapped (see Batches), leaving a submission descriptor (RVKS) in the DMA buffer
of a `D3DKMTSubmitCommandVirtual`, and letting the miniport ring the doorbell;
the miniport completes the D3DKMT submission fence the process waits on from the
fence-page interrupt (see Fence page). A reply-generating command's answer is in
the reply slot before the fence completes, so a process that waited reads a
finished answer.

## Host-visible bind record (user mode -> guest driver, escape)

The private data of one `D3DKMTEscape` whose `hContext` names a live rvk
context (feature 045). A process binds a chunk of its host-visible memory - an
allocation of kind `host_visible_memory`, resident and locked - to the context,
and the miniport publishes the chunk's pages as a `GUEST` blob in one of the
context's 64 chunk slots, which IMPORT_BLOB then names. The miniport decodes it
at `PASSIVE` and treats every byte as hostile; it writes the answer back into the
same bytes. 32 bytes.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `RVKH` = `0x484B5652` |
| 4 | u16 | version | 1 |
| 6 | u16 | operation | 1 bind, 2 unbind |
| 8 | u32 | allocation | the chunk's D3DKMT handle in the calling process |
| 12 | u32 | resource_id | bind: 0 in, the chunk's resource out; unbind: the resource to release |
| 16 | u64 | pages | 0 in; bind: the chunk's page count out |
| 24 | u64 | reserved | 0 |

An escape whose data is not this record, or that names no context, answers
`STATUS_NOT_SUPPORTED` as every other escape does. After that every refusal is
`STATUS_INVALID_PARAMETER` and sends nothing: a wrong version, operation or
reserved field; an allocation not of this process or not of kind
`host_visible_memory`; pages not complete; the context's chunk slots full; an
unbind of a resource not bound on the context. A bind sends
`RESOURCE_CREATE_BLOB` (`GUEST`, the pages as memory entries); an unbind sends
`RESOURCE_UNREF`, which the device refuses while an import holds the blob.
Tearing a context down unbinds its chunks before its own three resources, and a
chunk freed while bound destroys its context, as a freed ring does: the host may
still hold the pages imported.

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
| `RESOURCE_UNREF` of a blob a display head is showing | `ERR_INVALID_PARAMETER` |

A device reset releases every display head, then every blob and mapping.

## Blob scanout (guest -> device)

A display head shows a blob directly: the blob's guest pages become one
dma-buf and the host's display samples them, so no frame is transferred. The
guest may send `SET_SCANOUT_BLOB` only when the capset carries
`feature_dmabuf_scanout`; without it the head keeps `SET_SCANOUT`,
`TRANSFER_TO_HOST_2D` and `RESOURCE_FLUSH` over a 2D resource, which is what
every host started without a GL-capable display gives.

**`SET_SCANOUT_BLOB`** (96 bytes; virtio 1.2 5.7.6.10) binds head `scanout_id`
to blob `resource_id`, showing the rectangle `r` of a `width` x `height`
framebuffer of `format` at `offsets[0]` with `strides[0]`. Only plane 0 is
carried: a non-zero stride or offset on planes 1 to 3 is refused rather than
dropped, because the display shows one plane. `resource_id` 0 hides the head,
twice in a row without error. A head already showing something is not hidden
first - the new descriptor is taken before the old one is let go, so nothing
blank reaches the screen between two pictures.

| Refusal | Answer |
|---|---|
| shorter than 96 bytes, a non-zero `padding`, or a second plane | `ERR_INVALID_PARAMETER` |
| `scanout_id` at or beyond 16 | `ERR_INVALID_PARAMETER` |
| a format outside the eight 32-bit colour formats, a zero `width` or `height`, a stride below `width` x 4 or above `INT32_MAX` | `ERR_INVALID_PARAMETER` |
| a framebuffer whose last row runs past the blob | `ERR_INVALID_PARAMETER` |
| `r` outside the framebuffer, or with a zero side | `ERR_INVALID_PARAMETER` |
| `resource_id` naming no blob | `ERR_INVALID_RESOURCE_ID` |
| a display that will not show a dma-buf, a head the host does not have, or a blob whose memory cannot become one | `ERR_UNSPEC` |

The rule behind the table: `ERR_INVALID_PARAMETER` is the guest being wrong and
`ERR_UNSPEC` is the host declining. A refusal changes nothing - the head goes on
showing what it showed, and no descriptor is opened.

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

**The out-of-band raise** (feature 052). A host API that finishes work on a
thread the device never gave it - OpenCL's event callbacks - reaches the guest
through this page too, and the shape it is allowed is exactly what the guest's
history check already accepts: **`progress` moves, `completed_fence` does not,
and the interrupt is raised.** Nothing completed, because no batch did. The
foreign thread only sets a flag and wakes the context's worker, the way the
doorbell's ioeventfd does; the worker writes the page. Raises coalesce, so a
burst of host completions is one move of `progress` and one interrupt, and the
page never says which event finished - there is no room for one and no need,
since the guest asks over the wire. A faulted context raises nothing.

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
its probe: `vkRvkForceContextFault(VkPhysicalDevice)`, an ICD-local
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

The driver MUST write every fence queue's `queue_msix_vector` before `DRIVER_OK`.
The device has no `guest_notifier_mask`, so QEMU attaches a queue's irqfd only
when its vector is set before `DRIVER_OK` or when that MSI-X entry is unmasked; a
vector written later, to an entry the guest already unmasked, gets a route and
no irqfd, and every interrupt on it is lost (`hw/virtio/virtio-pci.c`,
`kvm_virtio_pci_vector_use_one`). `rvkgpu.sys` assigns them all at start and
writes the same value again when a context is set up. Feature 044 found this
live: each packet after a context's first completed only when an unrelated DPC
served it, 2-3 s later, past TdrDelay under load; qtest has no irqfd and could
not show it.

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
| 0 | u32 | magic | `RVKR` = `0x524B5652` |
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
| 0 | u32 | magic | `RVKD` = `0x444B5652` |
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
| 6 | blob | 32 bytes: u32 action (0 created, 1 imported, 2 released), u32 memory_type, u64 blob_id, u64 offset, u64 size | a blob is created, imported into a context, or released |
| 7 | blob bytes | 16 bytes: u64 blob_id, u64 offset; then the snapshot | an import is recorded and this kind was asked for |

A batch's payload is the same sequence of framed commands *Execution* above
describes, so a reader walks it with the same header. A recording holds what the
executor library received, which is the guest's bytes after the device
validated the framing around them and nothing more: the bytes inside a command
are still the guest's, and every tool that reads a recording treats them as
hostile.
