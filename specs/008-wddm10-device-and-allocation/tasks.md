---
description: "Task list for feature 008: WDDM 1.0 devices, allocations and segments"
---

# Tasks: WDDM 1.0 devices, allocations and segments

## Phase 1: Pool

- [X] T001 Constitution 2.2.0 (III: `ObjectPool<T>`); `CLAUDE.md`, `.claude/commands/dev.md`, `docs/testing.md` wording; `tools/check_design.py` allows placement new in `src/core/object_pool.hpp` (test-first in `tests/tools/test_check_design.py`)
- [X] T002 Tests `test_object_pool.cpp` (create across chunks, destroy, find rejects foreign/interior/destroyed addresses, destroy_all, allocation failure, invariants after every mutation) - must fail
- [X] T003 Implement `ObjectPool<T>`

**Checkpoint**: commit.

## Phase 2: Records

- [X] T004 Records for CREATEDEVICE, ALLOCATIONINFO, CREATEALLOCATION, DESTROYALLOCATION, DESCRIBEALLOCATION, GETSTANDARDALLOCATIONDRIVERDATA, OPENALLOCATIONINFO, OPENALLOCATION, CLOSEALLOCATION, SETDISPLAYPRIVATEDRIVERFORMAT, QUERYSEGMENTOUT4, SEGMENTDESCRIPTOR4, and the four standard surface data structures; km proofs; Linux size tests

## Phase 3: Descriptor and standard allocations (US2)

- [X] T005 Tests then `AllocationDescriptor` (encode/decode, hostile sweep, round trip) in `src/proto`; `docs/protocol.md`; `fuzz_proto` case
- [X] T006 Tests then `StandardAllocation` (each type and GDI surface type, unsupported formats and types, overflow, size-only query)

**Checkpoint**: commit.

## Phase 4: Segment (US1)

- [X] T007 Tests then `AdapterInfoQuery::read_output`, `ApertureSegment`, `QUERYSEGMENT4` dispatch; system memory from `DXGK_DEVICE_INFO`
- [X] T008 `FakeVidMm` and its rule tests; fake device info carries system memory

**Checkpoint**: commit.

## Phase 5: Devices and allocations (US3, US4)

- [X] T009 Tests then `DxgkrnlInterface::get_handle_data`; fake adapter handle table and callback
- [X] T010 Tests then `Allocation`, `AllocationTable`
- [X] T011 Tests then `Device`, `DeviceAllocation`, `DeviceTable`, `DisplaySources`
- [X] T012 Adapter DDIs, entry points, nine slots, `test_miniport`; allocation-failure sweeps; `fuzz_allocation_ddis` with corpus

**Checkpoint**: commit.

## Phase 6: Scenarios

- [X] T013 `FakeDxgadapter::render_core_rule`; scenarios: render core partial naming SubmitCommand, segments after start, a primary's life through the table, DestroyDevice with opens, RemoveDevice with devices outstanding
- [X] T014 Docs (`docs/ddi-sequences.md` render core and VidMm segments, `docs/protocol.md`); `tools/gate.sh`; merge
