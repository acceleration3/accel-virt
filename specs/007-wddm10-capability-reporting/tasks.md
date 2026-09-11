---
description: "Task list for feature 007: WDDM 1.0 capability reporting"
---

# Tasks: WDDM 1.0 capability reporting

## Phase 1: Records

- [X] T001 DDI records `DxgkargQueryAdapterInfoRecord`, `DxgkDriverCapsRecord`, `DxgkWddmDeviceCapsRecord`, `DxgkargEscapeRecord`, `DxgkargCollectDbgInfoRecord`, `DxgkargCollectDbgInfoExtRecord` in `src/core/ddi/`; every field offset and size proven in `src/km/ddi_record_layout.cpp`
- [X] T002 Test then `NtStatus::not_implemented` (0xC0000002)

## Phase 2: QueryAdapterInfo (US1, US2)

- [X] T003 Tests `test_adapter_info_query.cpp` (null pointers, short input, short output, exact write, hostile sweep over sizes) - must fail
- [X] T004 Implement `AdapterInfoQuery`
- [X] T005 Tests `test_driver_capabilities.cpp` (0x250 bytes, WDDMv1, all ones address, every other byte zero, device caps) and `Adapter::query_adapter_info` in `test_adapter.cpp` (every type 0-48 and out of range, before start, after stop, at D3, null block) - must fail
- [X] T006 Implement `DriverCapabilities`, `Adapter::query_adapter_info`, `AvDdiQueryAdapterInfo`, slot 17; `test_miniport` slot list
- [X] T007 `tests/fuzz/fuzz_adapter_info.cpp` and seed corpus. Found: core reaching `src/proto` must include `../proto/...`; the kernel build has only `src/core` on its include path

**Checkpoint**: commit (Phases 2 and 3 share one: the escape and debug tests landed in the same adapter test file).

## Phase 3: Escape and CollectDbgInfo (US4)

- [X] T008 Tests `test_debug_report.cpp` and adapter escape/collect tests (null/zero/any size, truncation to whole fields, extension zeroed, no allocation at DISPATCH) - must fail
- [X] T009 Implement `DebugReport` (over `WireWriter`), `Adapter::escape`, `Adapter::collect_dbg_info`, entry points, slots 34 and 35

## Phase 4: Harness and scenarios (US3)

- [X] T010 `tests/harness/capability_rule.hpp`, `FakeDxgadapter`; tests in `test_fake_dxgadapter.cpp` that a crafted `DRIVERCAPS` breaking each rule is refused, and that a status outside the accepted four fails the test. Found: a WDDMv2 answer meets MultiEngineAware (after PHYSICALADAPTERCAPS) before any static check, so crafted v2 answers set it
- [X] T011 `FakeDxgkrnlFdo::start_device` runs `FakeDxgadapter` after QueryChildRelations and stops on refusal; scenarios: bring-up validates capabilities, every query type through the table, a refused capability stops the device
- [X] T012 Docs: `docs/testing.md` and `docs/ddi-sequences.md` §2.4/§5 (accepted statuses, DRIVERCAPS rules, FDO step); `tools/gate.sh`; merge

**Checkpoint**: commit, gate, merge.
