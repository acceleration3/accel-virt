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
- [X] T004 [US1] A guest run: read where `rvkgpu.log` stops, and record it in this file as the defect Phase 2 fixes

**The defect Phase 2 fixes**, read from `dxgkrnl.sys` in `re/ghidra-proj` rather
than guessed: `DXGADAPTER::Initialize` @`1401a2818` returns
`STATUS_OBJECT_NAME_NOT_FOUND` from its one literal `0xC0000034`, the check
logging "UMD name is missing and device is not compute only" - taken when
`UmdNameMissing` is set and `MiscCaps & 0xC` is clear. `DpiGetAdapterInfo`
@`14024eb50` sets `UmdNameMissing` when `DxgkRetrieveStringFromRegistry`
@`140250b00` cannot read `UserModeDriverName` from the software key
(`DpiOpenPnpRegistryKey(dev, 2, ...)`, i.e. `HKR` under `[Rvkgpu_Install]`, not
under `.HW`). `DxgkAddAdapter` @`140435870` passes the status up unchanged and
`DpiFdoStartAdapter` @`14025a7c8` reports reason 3,
`StartAdapter_AddAdapterFailed`, which is Event 549's text. The value must be
`REG_SZ` or `REG_MULTI_SZ`, non-empty; nothing loads the DLL at adapter start,
so naming `rvkumd.dll` before feature 055 builds it is correct. The only other
way past the check is `MiscCaps.ComputeOnly`, which a display miniport must not
claim.

**Checkpoint**: commit.

## Phase 2: The adapter starts

- [X] T005 [US3] An L2 scenario in tests/harness/dxgkrnl/ reproducing the adapter-start refusal, citing the `dxgkrnl.sys` function it was read from; it fails before the fix

  Not an L2 scenario: the fake `dxgkrnl` reads no registry, so nothing in the
  harness can miss a value the INF never wrote. The reproducing test is a
  package rule in `tests/tools/test_driver_package.py` citing
  `DXGADAPTER::Initialize`, `DpiGetAdapterInfo` and
  `DxgkRetrieveStringFromRegistry`; it failed with `KeyError: 'AddReg'` before
  the fix - `[Rvkgpu_Install]` had no `AddReg` at all - and passes after.
- [X] T006 [US1] The fix in core, with its L1 tests; `driver_image` gains a rule if the kernel refused the image before `DriverEntry` (FR-002)

  The fix is in the package, not in core, and `driver_image` gains nothing: the
  kernel loaded the image and ran `DriverEntry`. What refused was dxgkrnl, after
  `AddDevice`, for a missing software-key value. `src/km/rvkgpu.inf` now adds
  `UserModeDriverName` (`REG_MULTI_SZ`, `rvkumd.dll`) under `[Rvkgpu_Install]`.
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
