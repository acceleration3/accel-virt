# Data model: Sharing across APIs and processes (053)

## SharedObjectKind

One enum, four members, shared by the wire, the miniport and the device. A kind
is part of an object's identity, never inferred: an image opened as a fence
would let a guest read a `VkSemaphore` as memory.

| Value | Name | What it is |
|--:|---|---|
| 1 | `memory` | an allocation whose bytes are shared |
| 2 | `monitored_fence` | a monitored fence's value page |
| 3 | `native_fence` | a WDDM 3.1 native fence |
| 0, 4+ | - | refused everywhere |

## SharedObjectRecord — 40 bytes, allocation private driver data

Written by the miniport at create, read back by the miniport at open. It
follows the 48 bytes of `AllocationDescriptor`; private data of exactly 48
bytes is an allocation that is not shared, which is every allocation before
this feature.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `RVKS` = `0x534B5652` |
| 4 | u32 | version | 1; anything else is refused |
| 8 | u64 | object_id | non-zero, minted by the adapter |
| 16 | u32 | kind | a `SharedObjectKind` |
| 20 | u32 | reserved | 0 |
| 24 | u64 | adapter_luid | the adapter that minted it |
| 32 | u64 | size | non-zero, a whole number of 4096-byte pages |

## SharedObjectRequest — 24 bytes, the body of transport commands 4, 5 and 6

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u64 | object_id | non-zero |
| 8 | u32 | kind | a `SharedObjectKind` |
| 12 | u32 | reserved | 0 |
| 16 | u64 | wire_handle | SHARE: the object in this context, non-zero. OPEN: 0. RELEASE: 0 |

## SharedObjectAnswer — 16 bytes

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | status | 0 ok |
| 4 | u32 | reserved | 0 |
| 8 | u64 | wire_handle | OPEN: what this context names it by, never 0 when ok. SHARE and RELEASE: 0 |

| status | refused because |
|--:|---|
| 0 | not refused |
| 1 | a reserved field is not 0, the id is 0, the kind is not one, or a wire handle is set where it may not be |
| 2 | SHARE: the wire handle is not one this context's executor made. OPEN/RELEASE: no such object |
| 3 | the kind, or the size, disagrees with the object the device holds |
| 4 | the executor refused to export or import it, or its library is older than ABI version 6 |
| 5 | the device or the context holds as many shared objects as it can, or the host has no memory for one |
| 6 | SHARE: this id is already published with a different descriptor. OPEN: this context already holds it |

## SharedObject — the miniport's, one per shared allocation

| Field | Rule |
|---|---|
| `id_` | non-zero, unique in the table, never reused while the table lives |
| `kind_` | a `SharedObjectKind` |
| `size_` | the allocation's page-rounded size |
| `allocation_` | the `Allocation` it names, for as long as it lives |
| `opens_` | devices holding it; 0 means only the creator's record remains |

**Invariants.** A live object has a non-zero id, a kind in range, a non-null
allocation, and a size that is a whole number of pages. `opens_` never exceeds
`Allocation::opens()`.

## SharedObjectTable — the miniport's, one per adapter

| Field | Rule |
|---|---|
| `next_id_` | starts at 1, only ever increases, never wraps back over a live id |
| `objects_` | an `ObjectPool<SharedObject>`; ids unique |
| `mutex_` | rank `shared_objects`, taken above the allocation table's |

**Operations.** `mint(allocation, kind, size, record)` writes the record;
`open(record, device)` finds the object the record names and refuses a bad
magic, version, kind, LUID, size or unknown id; `close(object, device)`;
`forget(allocation)` when the allocation is destroyed.

**Invariants.** Every live object's id is below `next_id_`, no two objects
share an id, and the count matches the pool's.

## HostSharedObject — the device's, one per published id

| Field | Rule |
|---|---|
| `id_` | non-zero |
| `kind_` | a `SharedObjectKind` |
| `blob_kind_` | `RVK_BLOB_DMA_BUF` or `RVK_BLOB_HOST_POINTER`: what the descriptor is |
| `size_` | non-zero |
| `descriptor_` | an fd this table owns; -1 when empty |
| `holders_` | contexts holding it, including the publisher |

**Invariants.** A live object holds a descriptor of at least 0 and at least one
holder; an empty slot holds no descriptor. The descriptor is closed exactly
once, when the last holder drops.

## HostSharedObjects — the device's, one per device

At most 256 objects, matching `ImportTable::max_imports`: a device that can
hold a thousand blobs can publish a quarter of them. Reached from every
context's worker, so every method takes the table's own host mutex, and
`report` prints `busy=true` rather than blocking the state dump.

## ContextSharedObjects — one per `DeviceContext`

At most 64, matching `ExternalObjects::max_objects` in the ICD: a guest that
published more than that could not have named them. Each row is an object id,
the wire handle this context's executor made, and whether this context
published it. Released with the context, which is what makes a context that
faults leave the object alive for everyone else (US3 scenario 2).

## icd::SharedHandles — one per ICD root

At most 64 rows: a Win32 handle, the object id behind it, and whether this
process minted it. A handle this process did not open and did not mint is not
in the table, which is the refusal 047 already gives, now for a real reason.
