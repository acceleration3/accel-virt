# Contract: host-visible memory without a copy

Design A of research.md §7. Three hops, each validating what the one before it
could have lied about.

## 1. ICD to KMD: binding a chunk (escape)

The ICD creates an allocation whose descriptor kind is `host_visible_memory`
(a new `AllocationKind`, pinned in the aperture segment like `context_ring`,
`AccessedPhysically`, any page count), makes it resident and locks it. It then
calls `D3DKMTEscape` with `hContext` naming its rvk context and this private data
(little-endian, 32 bytes):

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `RVKH` = `0x484B5652` |
| 4 | u16 | version | 1 |
| 6 | u16 | operation | 1 bind, 2 unbind |
| 8 | u32 | allocation | the D3DKMT handle of the chunk, in the calling process |
| 12 | u32 | resource_id | out: the device resource the chunk became (bind); in: the one to release (unbind) |
| 16 | u64 | pages | out: the chunk's page count (bind); 0 in |
| 24 | u64 | reserved | 0 |

The KMD resolves the allocation through `DxgkCbAcquireHandleData` in the calling
process, requires kind `host_visible_memory`, complete pages, a live rvk context
for `hContext` and a free chunk slot on it, and sends `RESOURCE_CREATE_BLOB`
(`GUEST`) with the pages as memory entries. Refusals answer
`STATUS_INVALID_PARAMETER`. Resource ids are the context's own range: a context
has 3 transport resources and 64 chunk slots. Tearing the context down, or freeing
the allocation, unbinds.

## 2. ICD to device: `IMPORT_BLOB` (transport command, API 0)

| Id | Command | Body | Answer |
|--:|---|---|---|
| `0x00000002` | IMPORT_BLOB | u32 resource_id, u32 reserved, u64 offset, u64 size, u32 memory_type, u32 reserved | u32 status (0 ok), u32 reserved, u64 device_memory (wire handle) |
| `0x00000003` | RELEASE_BLOB | u64 device_memory | empty |

The device requires the resource to be a GUEST blob of this context's range,
`offset + size` inside it without overflow, `offset` and `size` whole pages. It
builds (once per blob) a `GuestPageAlias` - one host range over the blob's pages -
and calls the executor's `import_blob` with `RVK_BLOB_HOST_POINTER` at
`alias + offset`. A refusal anywhere answers a non-zero status and does not fault
the context. RELEASE_BLOB of a handle this context never imported answers empty.

## 3. Device to executor: the import

`import_blob(blob_id, {HOST_POINTER, pointer, -1, size})` on the context's
worker. `VulkanBackend::adopt` imports the pointer into the context's device as
`memory_type` (refusing a type not in the host-pointer import mask), registers the
resulting `VkDeviceMemory` as a handle the guest may name, and the device answers
it. `vkFreeMemory` of that handle releases the import rather than freeing it twice.

## 4. What the guest sees

`vkAllocateMemory` on a host-visible type answers that `VkDeviceMemory`;
`vkMapMemory` answers the chunk's lock view plus the offset, with no round trip.
Bytes the host GPU writes to the memory are the bytes the view reads.
