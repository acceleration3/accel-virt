---
description: "Task list for feature 037: real-guest bring-up"
---

# Tasks: Real-guest bring-up: the in-guest suite passes

Tests come first in every task and must fail before the code they cover exists.
Phases 2 and 3 are a loop: each defect Windows exposes is reproduced at L2
before it is fixed, citing the `dxgkrnl.sys` function the belief was read from.

## Phase 1: The driver says where it stops

- [X] T001 [US4] Tests then `LogBudget` in src/core/platform/log_budget.{hpp,cpp}: lines per window, the window rolling over, a burst refused, no allocation and no block at `Irql::dispatch`
- [X] T002 [US1] [US4] The registration status logged when `register_miniport` refuses, in src/core/miniport/driver_image.hpp; L1 test over `MockPlatform` asserting the line and its status value
- [X] T003 [US1] [US4] One line each for `AddDevice`, `StartDevice` and every `QueryAdapterInfo` type answered or refused, in src/core/miniport/miniport.cpp and src/core/adapter/adapter.cpp; the DPC's existing refusal lines go through `LogBudget`; L1 tests assert each line once
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

  **The second defect, found by the marker run.** With `UserModeDriverName` in
  place the adapter starts: the log reaches `StartDevice`, `QueryChildRelations`
  and twenty-two `QueryAdapterInfo` answers, then the guest bugchecks
  `0x113` (VIDEO_DXGKRNL_FATAL_ERROR) subcode 7, parameter 3 `0xC0000005`.

  Read from the binaries, not guessed: dxgkrnl never calls `KeBugCheckEx` with
  `0x113` - it goes through `watchdog.sys`'s `WdLogSingleEntry5` @`1400011c0`,
  whose first argument 0 means bugcheck. The one site matching these parameters
  is `DXGK_VIDPNTARGETMODESET_INTERFACE_V1_IMPL::AddModeImpl` @`1403b38ec`, the
  call at @`1403b40a1`, where `0xC0000005` is a hardcoded immediate and
  parameter 2 is dxgkrnl's own VidPn target object - so parameter 3 is *not* a
  captured exception code. The guard: on the V1 interface a target mode whose
  `MinimumVSyncFreq.Numerator`/`.Denominator` (offsets `+0x44`/`+0x48`) are not
  the all-ones `CreateNewModeInfo` stamped is a bugcheck, not a returned status.

  `VidPnModeSet::add` copies the caller's description over the whole record
  (`*info = description`), and nothing in `src/` ever wrote `MinimumVSyncFreq`,
  so both fields reached dxgkrnl as zero. Reproduced in the harness -
  `FakeModeSet::create_new` now stamps what dxgkrnl stamps and `add` fails the
  test when it comes back changed - and fixed in `CofuncModality::offer_targets`.

**Checkpoint**: commit.

**The third defect, found by reading the marker run's own log.** The VidPn fix
holds live - `EnumVidPnCofuncModality` and `IsSupportedVidPn` both answer
`0x00000000`, repeatedly, where the driver used to bugcheck - but the log
itself came back interleaved between vCPUs:

```
rvkgpu: QruvekrgypAud:a pEtneurmIVnifdoP ntCyopfeu n2c4M osdtaaltiutsy  0sxtaCt0u0s0 000x002000000
rvkgpu:rv kQgupeur:y AIdsaSputpeproIrntfeod VtiydpPen  2s5t atsutsa t0uxs0 000x0C00000000
```

Two processors writing the debug port at once, character by character. It also
truncates lines. US1 acceptance 2 asks the log to show the steps *in order* and
FR-003 asks each refusal to be one line, so the instrument this feature depends
on has to be serialized: the platform owns the log device, so the write belongs
under a lock there. `LogBudget` cannot cover it - it releases its lock before
the write, and the lines outside the budget never take it.

**The fourth and fifth defects, found by reading dxgkrnl for the commit loop.**
The live trace ends with `IsSupportedVidPn` and `EnumVidPnCofuncModality`
alternating forever and `CommitVidPn` never called. Each pair is a *fresh
mutation attempt*, not a retry: `VIDPN_MGR::FormalizeVidPnChange` @`1403056f4`
runs after every VidPn mutation and calls both.

*Fourth*: `CofuncModality::update_support` gates `pfnUpdatePathSupportInfo` on
`ContentTransformationScaling/Rotation == 0` (UNINITIALIZED), but a live path
carries **254** - `D3DKMDT_VPPS_UNPINNED` / `D3DKMDT_VPPR_UNPINNED`. Proven
twice: `AcquirePathInfo` @`1403a6b10` builds the default path with `0xFE` in
both fields, and `PinPathContentScaling` @`1403052c0` rolls back with
`if (saved == 0xfe) UnpinContentScaling()`. So we never call
`pfnUpdatePathSupportInfo`, the support mask at `DMMVIDPNPRESENTPATH+0x78`
stays 0 (written only from `UpdatePathSupportInfo` @`1403ab840`), and
`PinContentScaling` @`140013e5c` fails `0xC01E0306` forever, so
`CommitVidPnOnAdapter` @`1402f82a4` never reaches `CommitVidPn`. dxgkrnl also
expects **one** `pfnUpdatePathSupportInfo` per path per enumeration: the
countdown at `@1403ab840` only calls `DisallowModifyingAction` when it hits 0.

*Fifth*: `offer_targets` assigns an **empty** target mode set whenever
`modes_.find(*pinned_source)` answers null. `DMMVIDPNTARGET::SetCofuncModeSet`
@`14005004c` refuses an empty set with `0xC01E0309` (the source side,
@`14002fffc`, refuses with `0xC01E0308`).

Both were reproduced in the harness first - `add_path` now presents `254` as
dxgkrnl does, and `assign_source`/`assign_target` refuse an empty set with
`0xC01E0308`/`0xC01E0309`, leaving it to the driver to release - which failed 4
of 10 cofunc modality tests, and both are now fixed:

- The fourth: `update_support` treats both `0` (UNINITIALIZED) and `254`
  (UNPINNED) as unpinned, and calls `vidpn_.update_path_support` on **every**
  path, not only when something was offered.
- The fifth: `offer_targets` returns without creating a set when the pinned
  source mode has no match, instead of assigning an empty one.

Three tests encoded the wrong belief and were rewritten:
`a_path_with_both_transformations_chosen_is_updated_offering_nothing` (dxgkrnl
counts one update per path per enumeration, so a pinned path is updated - it
just offers nothing), `a_pinned_source_mode_the_adapter_lacks_leaves_the_target_set_alone`,
and `vidpn_create_mode_set.answers_fresh_sets_to_fill_and_assign`, which used to
assign the empty sets it had just created. `an_uninitialized_transformation_is_offered_identity`
covers the other unpinned value.

**The sixth defect, found by the guest run the fourth and fifth fixes made
possible.** With `pfnUpdatePathSupportInfo` finally reaching dxgkrnl, the guest
bugchecked `0x113` (VIDEO_DXGKRNL_FATAL_ERROR) subcode `0x1C` one second into
the device restart, with no bugcheck parameters on the wire and no kernel dump.
Read off the paused guest over QEMU's gdbstub: `nt!KiBugCheckData` held
`0x113, 0x1C, 0x1, 0x1, 0x0`, and the stack of the bugchecking processor carried
`VidPn::update_path_support` -> `CofuncModality::update_support` ->
`CofuncModality::visit` -> `VidPn::for_each_path` -> `AvDdiEnumVidPnCofuncModality`.

`DMMVIDPNPRESENTPATH::SetRotationSupport` (dxgkrnl.sys @`14004b80c`) validates
the rotation support mask whenever `IsDriverSupportPathIndependentRotation`
(@`1403d9d78`) is true - which it is for any driver whose declared interface
version is above `0x4002`, so always for us - and the machine is test-signed. A
primary clone path, which `IsPrimaryClonePath` (@`14027ce7c`) makes the only
path of a single-path topology, must carry `Offset0` (`0x10`) and none of
`Offset90/180/270`. Parameter 2 of the bugcheck is the mask we sent: `0x1`,
Identity alone. `basicdisplay.sys`'s own `EnumVidPnCofuncModality` ends its
rotation mask with `| 0x10` for the same reason.

`update_support` now states what the adapter actually does - identity scaling,
identity rotation at offset zero - on every path in one call, and leaves a path
whose scaling or rotation is the enumeration's pivot alone entirely, because one
call writes both supports and there is no way to update one and not the other.
`FakeVidPn::update_path` fails the test on a mask dxgkrnl would bugcheck on, so
this cannot come back as a guest bugcheck.

`a_storm_of_refusals_is_silent_once_the_window_is_spent` closes T003's last
uncovered branches: with the window spent, the four DPC refusal lines and every
`log_status` line say nothing. The one line the DPC still writes is
`VirtioGpuDevice::harvest` failing the device, which happens once and then stops
the queues - not a storm to budget.

## Phase 3: The suite passes

- [ ] T008 [US2] Each `rvktest.exe` scenario that fails - adapter, allocation, GPU mapping, submission - reproduced at L2, fixed in core, re-run, one commit per defect
- [ ] T009 [US2] `D3DKMTEnumAdapters2` lists the adapter at WDDM 3.2 with its PCI identity; no Verifier violation, no TDR, no live kernel report caused by `rvkgpu`

**Checkpoint**: commit.

## Phase 4: It stays passing

- [ ] T010 [US2] Three consecutive runs end with verdict 0, their `diag/` free of start errors, bugchecks and Verifier violations
- [ ] T011 `tools/gate.sh` with `AV_GUEST_IMAGE` set becomes the merge gate from this feature on (FR-006); docs/testing.md and docs/roadmap.md; 035's T016 closes; merge; push

**Checkpoint**: commit.
