# Implementation Plan: WDDM 1.0 devices, allocations and segments

**Branch**: `008-wddm10-device-and-allocation` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

Nine slots join the table. `ObjectPool<T>` (constitution 2.2.0) gives devices,
allocations and per-device opens stable addresses that serve as dxgkrnl
handles. VidMm is told of one aperture segment over guest system memory; the
runtime's standard allocations and user-mode allocations are described by a new
rvk structure, `AllocationDescriptor`. The harness models VidMm's segment
queries and `CreateRenderCore`'s DDI rule.

## Design

- **`ObjectPool<T>`** (`src/core/object_pool.hpp`): chunks of 64 slots from
  `Platform::allocate(non_paged)`, a free bitmap per chunk, `create` /
  `destroy` / `find(void *)` (null unless the address is a live object of this
  pool) / `destroy_all`. The pool holds no lock; its owner does.
- **Ownership**: `AllocationTable` owns allocations and the rule "an allocation
  with opens cannot be destroyed"; `DeviceTable` owns devices and opens and
  calls into `AllocationTable` to count opens. Lock ranks: device table 1,
  allocation table 2 (`lock_rank.hpp`).
- **DDI signatures**: `DestroyDevice`, `OpenAllocation` and `CloseAllocation`
  receive the device handle, so `Device` remembers its `Adapter` and the entry
  point forwards through it.
- **Segments**: `ApertureSegment::initialize(system memory)` at StartDevice;
  `report(AdapterInfoQuery)` for `QUERYSEGMENT4`, which reads its in/out block
  through a new `AdapterInfoQuery::read_output`.
- **Handle data**: `DxgkrnlInterface::get_handle_data(D3DKMT_HANDLE)` resolves
  an allocation during OpenAllocation; the fake adapter keeps a handle table.
- **Harness**: `FakeVidMm` (both passes, `ValidateSegmentDescriptors`),
  `FakeDxgadapter::render_core_rule()` and `missing_render_slot()`,
  `FakeDxgkrnlAdapter` handle table and `DxgkCbGetHandleData`.

## Phases

1. **Pool** - constitution 2.2.0, `check_design.py` allowance (test-first),
   `ObjectPool<T>` with allocation-failure and hostile-handle tests.
2. **Records** - 13 DDI records, km proofs.
3. **Descriptor and standard allocations** - `AllocationDescriptor` (proto,
   hostile sweep, `fuzz_proto` target), `StandardAllocation`.
4. **Segment** - `AdapterInfoQuery::read_output`, `ApertureSegment`,
   `QUERYSEGMENT4` in `query_adapter_info`, `FakeVidMm`.
5. **Devices and allocations** - `Allocation`, `AllocationTable`, `Device`,
   `DeviceAllocation`, `DeviceTable`, `DisplaySources`, `get_handle_data`,
   adapter DDIs, entry points, slots; `fuzz_allocation_ddis`.
6. **Scenarios** - render core rule, primary lifetime through the table, docs,
   gate, merge.

## Constitution Check

| Principle | Status |
|---|---|
| III | Amended 2.2.0 (user decision): `ObjectPool<T>` is the second placement-new site |
| IV | Pass - tests first; pools swept for allocation failure; descriptors and argument blocks fuzzed |
| V | Pass - every handle checked against its pool; every size overflow-checked |
| VI | Pass - no capability changes; the partial render DDI set is named by the harness |
| VII | Pass - all nine DDIs PASSIVE; tables take mutexes with declared ranks |
