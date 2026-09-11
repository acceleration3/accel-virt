---
description: "Task list for feature 017: WDDM 2.0 GPU virtual addressing"
---

# Tasks: WDDM 2.0 GPU virtual addressing

## Phase 1: Records

- [X] T001 Records, km proofs, Linux layout tests

## Phase 2: Harness

- [X] T002 Tests then `FakeDxgadapter` WDDM 2.0 rules and ValidateWDDMCaps gating; `FakeGpuMmu`; `FakeVidMm` implicit segment

**Checkpoint**: commit.

## Phase 3: Scanout at device IRQL

- [ ] T003 Tests then `Platform` atomics; `DisplaySources` scanout atomic

## Phase 4: Capabilities

- [ ] T004 Tests then `GpuMmuGeometry`; `DriverCapabilities` 2.0 and PHYSICALADAPTERCAPS; adapter queries; node metadata; allocation segments

**Checkpoint**: commit.

## Phase 5: Paging

- [ ] T005 Tests then `PageTableEntry`, `PageTableUpdate`
- [ ] T006 Tests then `PagingCommand` (proto, documented) and the `PagingRequest` 2.0 operations; fuzz_paging

**Checkpoint**: commit.

## Phase 6: Contexts and submission

- [ ] T007 Tests then `RootPageTable`, context flags, processes, SubmitCommandVirtual, GetRootPageTableSize, SetStablePowerState; slots
- [ ] T008 Scenarios; docs; gate; merge; push
