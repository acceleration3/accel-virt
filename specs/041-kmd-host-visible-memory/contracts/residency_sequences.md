# Contracts: what VidMm drives, and what the driver sends

Two contracts: the DDI sequence the tests drive through the real table, and the
requests the driver puts on the control queue for them. Each is what the L2 and
L3 tests assert, in order.

## A. A host-visible allocation, created to destroyed

| # | dxgkrnl calls | The driver must |
|---|---|---|
| 1 | `DxgkDdiQueryAdapterInfo(DXGKQAITYPE_QUERYSEGMENT4)`, count pass | answer `NbSegment` 2 where the region is offered, 1 where it is not |
| 2 | the same, descriptor pass | fill segment 1 as today, and segment 2 with `CpuVisible | CacheCoherent`, `CpuTranslatedAddress` = the region's guest physical base, `Size` = `CommitLimit` = the region's size |
| 3 | `DxgkDdiCreateAllocation` with a descriptor whose `flag_cpu_visible` is set | validate the descriptor, create the blob, and report the allocation preferring segment 2 |
| 4 | `DxgkDdiBuildPagingBuffer(DXGK_OPERATION_TRANSFER)` into segment 2 at offset X | map the blob at X and record the placement |
| 5 | the process writes through its view | nothing: no command is sent per access |
| 6 | `DxgkDdiBuildPagingBuffer(DXGK_OPERATION_TRANSFER)` out of segment 2 | unmap the blob, release the placement, keep the blob |
| 7 | a later transfer back in, at another offset Y | map the blob at Y; what was written is still there |
| 8 | `DxgkDdiDestroyAllocation` | unmap if mapped, unref the blob, release the placement |

Where the region is not offered, step 2 reports one segment, step 3 places the
allocation in VidMm's own system memory and says so, and steps 4 to 8 never
name segment 2.

## B. The requests on the control queue

| Step | Request | Fields this feature pins |
|---|---|---|
| create | `RESOURCE_CREATE_BLOB` (0x010C) | `blob_mem = 2` (HOST3D), `flags = 1` (USE_MAPPABLE), `nr_entries = 0`, `blob_id = 0`, `size` = the allocation's size rounded to a page |
| map | `RESOURCE_MAP_BLOB` (0x0208) | `resource_id`, `offset` = the placement, page-aligned and wholly inside the region |
| the answer | `OK_MAP_INFO` (0x1106) | `map_info = 1` (`MAP_CACHE_CACHED`); anything else leaves the blob unmapped |
| unmap | `RESOURCE_UNMAP_BLOB` (0x0209) | `resource_id` |
| destroy | `RESOURCE_UNREF` (0x0102) | `resource_id` |

The device's own rules, which the L3 test drives against the real device and
which the driver must therefore never break: an offset that is misaligned,
outside the region, or overlapping a live mapping is
`ERR_INVALID_PARAMETER`; mapping an already-mapped blob is refused; a second
unmap is refused; contents survive an unmap and a map at another offset
(`docs/protocol.md:403-435`).

## C. What the guest and the host must agree on, live

The L5 check is the only one that can see both sides at once:

1. the runner starts QEMU with `hostmem=256M`, so the device offers the region;
2. the guest driver reports two segments, and the in-guest suite creates a
   host-visible allocation and writes a pattern through the view VidMm gives it;
3. the runner reads the BAR's guest physical range back through QMP and finds
   the same pattern.

Step 3 is what proves the memory is the host's: nothing in the guest could tell
the difference otherwise.
