---

description: "Tasks: KMD host-visible memory a process can map"
---

# Tasks: KMD host-visible memory a process can map

**Input**: Design documents from `/specs/041-kmd-host-visible-memory/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/residency_sequences.md, quickstart.md

**Tests**: Required, and written first. Constitution IV.

**Organization**: by user story, in priority order. Each `**Checkpoint**` is a commit once `ctest --test-dir build` passes there.

## Format: `[ID] [P?] [Story] Description`

- **[Story]**: US1 (host-visible memory with no copy), US2 (rings and fence pages a process can map), US3 (32-bit views and budgets)

---

## Phase 1: Foundational — the words the driver has never said

**Purpose**: the driver has never created a host blob, never mapped one, and never read `OK_MAP_INFO`. Nothing above can be built until it can.

- [X] T001 [P] Write `tests/unit/proto/virtio_gpu/test_rvk_request_writer.cpp` first for the three requests: a host blob is `RESOURCE_CREATE_BLOB` with `blob_mem = 2` (HOST3D), `flags = 1` (USE_MAPPABLE), `nr_entries = 0`, `blob_id = 0` and a page-multiple size; a map is `RESOURCE_MAP_BLOB` (0x0208) with the resource id and a 64-bit offset; an unmap is `RESOURCE_UNMAP_BLOB` (0x0209); each refuses a buffer too small without writing to it
- [X] T002 Implement `host_blob`, `map_blob` and `unmap_blob` in `src/proto/virtio_gpu/rvk_request_writer.{hpp,cpp}`
- [X] T003 [P] Write `tests/unit/proto/virtio_gpu/test_gpu_response_reader.cpp` first for `OK_MAP_INFO` (0x1106): the 32-byte answer decodes, `map_info` is readable, `MAP_CACHE_CACHED` is 1, a short answer is `malformed`, and a type that is not `OK_MAP_INFO` is not read as one
- [X] T004 Implement the `OK_MAP_INFO` case in `src/proto/virtio_gpu/gpu_response_reader.{hpp,cpp}`
- [X] T005 The adapter keeps the host-visible region the PCI walk found - `VirtioPciLayout::shared_memory(1)` - and `tests/unit/core/adapter/test_adapter.cpp` asserts an adapter whose device has no such region keeps none

**Checkpoint**: the driver can ask for host memory. Commit.

---

## Phase 2: User Story 1 - Host-visible memory mapped with no copy (Priority: P1) 🎯 MVP

**Goal**: a host-visible allocation is real host memory, placed in the hostmem region, addressable by the CPU through the BAR, with no command sent per access.

**Independent Test**: the L2 residency sequence in `contracts/residency_sequences.md`, the L3 requests against the real device, and the L5 read-back through QMP.

- [X] T006 [P] [US1] Write `tests/unit/core/memory/test_host_visible_segment.cpp` first: a segment is offered only when the capset sets `feature_host_visible_memory` **and** the region has a non-zero page-multiple size; its descriptor carries `Flags` = `CpuVisible | CacheCoherent` (`0x14`, never `Aperture`), `BaseAddress` 0, `Size` = `CommitLimit` = the region's size, `CpuTranslatedAddress` = the region's guest physical base, `SupportsCpuHostAperture` clear; an unoffered segment describes nothing; `forget()` leaves it unoffered
- [X] T007 [US1] Implement `HostVisibleSegment` (object class) in `src/core/memory/host_visible_segment.{hpp,cpp}`
- [X] T008 [P] [US1] Write `tests/unit/core/memory/test_memory_segments.cpp` first: `count()` is 1 without the region and 2 with it; the count pass of `DXGKQAITYPE_QUERYSEGMENT4` answers that; the descriptor pass fills each at the caller's stride and names the paging buffer on segment 1; `system_memory_segment_id()` is `count() + 1`; a stride smaller than a descriptor is refused
- [X] T009 [US1] Implement `MemorySegments` (system class) in `src/core/memory/memory_segments.{hpp,cpp}`, and `Adapter::query_adapter_info` answers `query_segment4` through it in `src/core/adapter/adapter.cpp`
- [X] T010 [P] [US1] Write `tests/unit/core/memory/test_hostmem_placement.cpp` first: a placement is page-aligned, wholly inside the region and never overlaps a live one; the offset is **VidMm's**, recorded and judged rather than chosen; a range another allocation holds is `conflict`; `release` frees exactly its owner's range and nobody else's; `holder` names which allocation holds a byte; every operation is PASSIVE and takes the mutex; invariants hold after every placement and release
- [X] T011 [US1] ~~Implement `HostmemPlacement`~~ **Built, then removed.** With the region mapped as one blob and VidMm owning the segment's address space, nothing in this feature asks which allocation holds which range; the constitution's rule against speculative structure says not to keep code with no caller. Feature 042's executor is the caller that will need it, and git holds the class and its tests
- [X] T012 [P] [US1] Write `tests/unit/core/memory/test_host_visible_blob.cpp` first over a fake device: `create` sends the host-blob shape and keeps the resource id; `map` sends the offset and accepts only `OK_MAP_INFO` with `MAP_CACHE_CACHED`; a refused map leaves the blob unmapped and the placement free; `unmap` keeps the blob and its contents; `destroy` unrefs; a blob with no resource id maps nothing
- [X] T013 [US1] Implement `HostVisibleBlob` (object class) in `src/core/memory/host_visible_blob.{hpp,cpp}`
- [X] T014 [US1] `Allocation::report_to` prefers the host-visible segment for an allocation whose descriptor sets `flag_cpu_visible` where the region is offered, and VidMm's system memory where it is not, in `src/core/memory/allocation.{hpp,cpp}`; `tests/unit/core/memory/test_allocation.cpp` asserts both, and that the primary still prefers the aperture with `AccessedPhysically`
- [X] T015 [US1] ~~`DXGK_OPERATION_TRANSFER` into the host-visible segment maps that allocation's blob at the offset it names~~ **The region is one blob, mapped at StartDevice.** Reading dxgmms2 settled it: `MapInCpuVisibleSegment` (@14009ea98) builds the MDL over the segment's CPU range itself, so VidMm maps a process's view and tells the driver nothing; and a GPU-MMU driver is issued no physical FILL or TRANSFER at all. There is therefore no call in which a per-allocation blob could be created in time, and the only shape that works is host memory behind *every* offset VidMm can choose: one blob the size of the region, created and mapped at offset 0 when the segment is offered, torn down at D3 and made again on the device D0 brings back
- [X] T016 [US1] ~~`DxgkDdiDestroyAllocation` unmaps, unrefs and releases~~ **Nothing to do:** an allocation owns no blob and no placement, so destroying one is what it already was. The region's blob belongs to the adapter and goes at D3 and StopDevice, which `close_region` does
- [ ] T017 [US1] The L2 residency scenario in `tests/unit/scenarios/test_ddi_scenarios.cpp`, driving the real table through contract A twice: an adapter whose capset offers the region, and one whose capset does not
- [ ] T018 [US1] The L3 requests against the real device in `tests/qemu/test_virtio_gpu_blobs.py`: the exact create, map, unmap and unref the driver emits, an offset it must never choose (misaligned, outside, overlapping) refused, and contents surviving an unmap and a map elsewhere

**Checkpoint**: a host-visible allocation is host memory. Commit.

---

## Phase 3: User Story 2 - Rings and fence pages a process can map (Priority: P1)

**Goal**: the pages an rvk context runs on can be allocations: pinned for the context's lifetime, and the owning process's alone.

**Independent Test**: L1 over `MockKmt`-style fakes and the L2 create/destroy sequence; mapping one into a real process belongs to feature 043 and is not claimed here.

- [ ] T019 [P] [US2] Extend `src/proto/resources/allocation_kind.hpp` and `allocation_descriptor.{hpp,cpp}` with the ring and fence-page kinds, and `tests/unit/proto/resources/test_allocation_descriptor.cpp` with the hostile table: a ring whose size is not a power of two page multiple, a fence page of the wrong size, a kind outside the enumeration, a size that overflows when page-aligned - each refused with nothing created
- [ ] T020 [US2] A ring or fence-page allocation is pinned for its context's lifetime and refused to a device that does not own it, in `src/core/memory/allocation.cpp` and `src/core/memory/allocation_table.cpp`, asserted in `tests/unit/core/memory/test_allocation_table.cpp`
- [ ] T021 [US2] `tests/fuzz/fuzz_allocation_descriptor.cpp`'s corpus gains the new kinds, and the fuzz smoke passes

**Checkpoint**: the transport's memory has a shape. Commit.

---

## Phase 4: User Story 3 - 32-bit views and budgets (Priority: P2)

**Goal**: what the adapter says about its memory is what the device can back, and a WoW64 process is never handed a truncated pointer.

- [ ] T022 [US3] The CPU-visible segment's `CommitLimit` never exceeds what the device backs, and `tests/unit/core/memory/test_host_visible_segment.cpp` asserts it against a capset that claims more than the region holds (FR-006)
- [ ] T023 [US3] Answer, in this file, whether the driver can influence where a WoW64 process's view lands: read `DXGK_SEGMENTDESCRIPTOR4`, `DXGK_DRIVERCAPS.HighestAcceptableAddress` and what dxgkrnl does with `CpuTranslatedAddress` for a 32-bit process (`dxgkrnl.sys` in Ghidra), and either implement the refusal FR-004 asks for or record why the placement is not the driver's to make

**Checkpoint**: the adapter tells the truth about its memory. Commit.

---

## Phase 5: What the feature promised

- [ ] T024 The in-guest suite gains a host-visible scenario in `src/guest/suite/`: create a CPU-visible allocation, write a pattern through the view `D3DKMTLock2` returns, read it back, and report what it wrote; `GdiKmt` gains the lock and unlock calls it needs, with `MockKmt` scripting every failure
- [ ] T025 The runner gains `--hostmem N` (default 256 MiB, `0` for a device with no region) in `tools/guest/run_guest_suite.py`, a `segments` step that reads the driver's own log line for what it reported, and the read-back: after the guest writes, the host reads that BAR range through QMP and compares; with its fake-guest tests
- [ ] T026 A live run with the region and one without it (SC-003); record what each showed here
- [ ] T027 `docs/protocol.md` documents capset feature bits 1 and 2 beside bit 0, and `docs/kb/04-memory.md` gains the segment shape this feature reports and why the aperture variant was refused
- [ ] T028 `AV_GUEST_IMAGE=... tools/gate.sh` passes; merge to `master`; push

---

## Dependencies & Execution Order

- **Phase 1** blocks everything: no class can be tested against a device it cannot talk to.
- **US1 (Phase 2)** is the feature; **US2 (Phase 3)** only needs the descriptor and
  the allocation table, so it can be built beside US1 once Phase 1 lands.
- **US3 (Phase 4)** depends on US1's segment existing.
- **Phase 5** depends on US1; T024 and T025 are the two halves of one live claim
  and land together.

### Parallel opportunities

- T001 and T003 are two wire tests in different files.
- T006, T008, T010 and T012 are four test files ahead of four classes.
- T019 is independent of everything in US1 after Phase 1.

---

## Implementation Strategy

**MVP**: Phases 1 and 2 - a host-visible allocation that is really the host's
memory, proven at L2, L3 and L5. **Increment 2**: Phase 3, the shape the
transport needs. **Increment 3**: Phase 4, the truth about budgets.

---

## Notes

- The hostmem region is the guest's to place into: the device only judges the
  offset, so every refusal in `docs/protocol.md:403-435` is a bug in this
  driver, not in the device.
- Nothing on the paging path may block on the device: a control exchange there
  stalls VidMm.
- A descriptor from user mode is hostile input, and a refusal is logged once
  with the offending value.
