---
description: "Task list for feature 037: real-guest bring-up"
---

# Tasks: Real-guest bring-up: the in-guest suite passes

Tests come first in every task and must fail before the code they cover exists.
Phases 2 and 3 are a loop: each defect Windows exposes is reproduced at L2
before it is fixed, citing the `dxgkrnl.sys` function the belief was read from.

## Phase 1: The driver says where it stops

- [ ] T001 [US4] Tests then `LogBudget` in src/core/platform/log_budget.{hpp,cpp}: lines per window, the window rolling over, a burst refused, no allocation and no block at `Irql::dispatch`
- [ ] T002 [US1] [US4] The registration status logged when `register_miniport` refuses, in src/core/miniport/driver_image.hpp; L1 test over `MockPlatform` asserting the line and its status value
- [ ] T003 [US1] [US4] One line each for `AddDevice`, `StartDevice` and every `QueryAdapterInfo` type answered or refused, in src/core/miniport/miniport.cpp and src/core/adapter/adapter.cpp; the DPC's existing refusal lines go through `LogBudget`; L1 tests assert each line once
- [ ] T004 [US1] A guest run: read where `rvkgpu.log` stops, and record it in this file as the defect Phase 2 fixes

**Checkpoint**: commit.

## Phase 2: The adapter starts

- [ ] T005 [US3] An L2 scenario in tests/harness/dxgkrnl/ reproducing the adapter-start refusal, citing the `dxgkrnl.sys` function it was read from; it fails before the fix
- [ ] T006 [US1] The fix in core, with its L1 tests; `driver_image` gains a rule if the kernel refused the image before `DriverEntry` (FR-002)
- [ ] T007 [US1] A guest run: the device reports status OK, problem 0, service `rvkgpu`, and `diag/kernel-pnp.txt` holds no 411 for it

**Checkpoint**: commit.

## Phase 3: The suite passes

- [ ] T008 [US2] Each `rvktest.exe` scenario that fails - adapter, allocation, GPU mapping, submission - reproduced at L2, fixed in core, re-run, one commit per defect
- [ ] T009 [US2] `D3DKMTEnumAdapters2` lists the adapter at WDDM 3.2 with its PCI identity; no Verifier violation, no TDR, no live kernel report caused by `rvkgpu`

**Checkpoint**: commit.

## Phase 4: It stays passing

- [ ] T010 [US2] Three consecutive runs end with verdict 0, their `diag/` free of start errors, bugchecks and Verifier violations
- [ ] T011 `tools/gate.sh` with `AV_GUEST_IMAGE` set becomes the merge gate from this feature on (FR-006); docs/testing.md and docs/roadmap.md; 035's T016 closes; merge; push

**Checkpoint**: commit.
