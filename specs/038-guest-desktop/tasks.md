---
description: "Task list for feature 038: real-guest desktop"
---

# Tasks: Real-guest desktop: DWM on rvkgpu at a chosen mode

Tests come first in every task and must fail before the code they cover exists.
Each guest run records what it found in this file; every disagreement is
reproduced at L2, citing the `dxgkrnl.sys` or QEMU function the belief was read
from, before it is fixed (FR-007).

## Phase 1: The wire requests the 2D path needs

- [ ] T001 [P] Tests then `set_scanout` in src/proto/virtio_gpu/gpu_request_writer.{hpp,cpp}: type `0x0103`, the scanout id, resource id and rectangle it writes, and a buffer one byte too small refused with nothing written
- [ ] T002 [P] Tests then `resource_flush` in src/proto/virtio_gpu/gpu_request_writer.{hpp,cpp}: type `0x0104`, the resource id and rectangle, and a short buffer refused
- [ ] T003 [P] Tests then `get_display_info` in src/proto/virtio_gpu/gpu_request_writer.{hpp,cpp}: type `0x0100` with a header and no payload, and a short buffer refused
- [ ] T004 Runner: a QMP `screendump` into the work directory and a comparison answering whether two dumps differ, in tools/guest/run_guest_suite.py; tests over the fake QEMU in tests/tools/fake_guest/

**Checkpoint**: commit.

## Phase 2 (US1): the committed frame reaches the host

- [ ] T005 [US1] Tests then `ScanoutResource` in src/core/display/scanout_resource.{hpp,cpp}: `CREATE_2D` and `ATTACH_BACKING` once per mode, `SET_SCANOUT` once per backing, `TRANSFER_TO_HOST_2D` plus `RESOURCE_FLUSH` only when the address the host was told differs from the recorded one, `invariants_hold` after every mutation, `Irql::dispatch` ceiling on the flush path
- [ ] T006 [US1] Tests then `DisplayScanout` in src/core/display/display_scanout.{hpp,cpp}: one `ScanoutResource` per source, distinct resource ids, a source out of range refused, teardown releasing every resource
- [ ] T007 [US1] `DisplayScanout` wired into src/core/adapter/adapter.cpp: `commit_vidpn` configures the committed source, `dpc_routine` flushes a source the host is behind on, `stop_device` tears the resources down; L1 tests over the fake virtio-gpu function assert the requests and their order
- [ ] T008 [US1] An L2 scenario in tests/unit/scenarios/test_ddi_scenarios.cpp: commit a VidPn, set the source address, run the DPC, and assert the device saw `CREATE_2D`, `ATTACH_BACKING`, `SET_SCANOUT`, `TRANSFER_TO_HOST_2D` and `RESOURCE_FLUSH` for that source
- [ ] T009 [US1] A guest run: screendumps before and after the guest opens a window differ, `dwm.exe` runs, the adapter is `rvkgpu` with status OK, Basic Display is not bound, and no display TDR is logged; record what it found here

**Checkpoint**: commit.

## Phase 3 (US2): modes from the device

- [ ] T010 [US2] Tests then `DisplayInfoResponse` in src/core/display/display_info_response.{hpp,cpp}: a valid answer parsed, and a hostile table - zero length, truncated at every field boundary, a count past the protocol's maximum, a rectangle whose width or height is zero or enormous, a disabled scanout, a byte-flip sweep - each answering a status and writing nothing; plus tests/fuzz/fuzz_display_info.cpp with a committed corpus
- [ ] T011 [US2] Tests then `DisplayModeList` built from a `DisplayInfoResponse` in src/core/display/display_mode_list.{hpp,cpp}: the device's size preferred, standard modes that fit offered, none that do not, and a refused or absent answer keeping the current list with the value logged
- [ ] T012 [US2] The adapter asks for display info at `start_device` and on a device configuration change, and the EDID follows the preferred mode, in src/core/adapter/adapter.cpp and src/core/display/edid.{hpp,cpp}; L1 and L2 tests
- [ ] T013 [US2] Runner: a guest-side mode list and switch over the agent, and a screendump whose size follows it; a mode the driver does not offer is refused and the current mode stays, in tools/guest/run_guest_suite.py
- [ ] T014 [US2] A guest run: every offered mode selected in turn is the screendump's size, the choice survives a reboot, and a host display-size change is followed; record what it found here

**Checkpoint**: commit.

## Phase 4 (US3): frame pacing

- [ ] T015 [US3] Tests then `VsyncScenario` in src/guest/suite/vsync_scenario.{hpp,cpp} over the `Kmt` contract: the vertical blanks counted in a window judged against the committed mode's rate, with `MockKmt` scripting a refusal, a timeout and a rate outside the tolerance
- [ ] T016 [US3] `wait_for_vertical_blank` added to src/guest/kmt/kmt.hpp and implemented in src/um/gdi_kmt.cpp over `D3DKMTWaitForVerticalBlankEvent`, with the struct offsets it reads pinned by `static_assert`; the scenario joins `GuestSuite`
- [ ] T017 [US3] A guest run: the rate is within 5% of the committed mode over 60 s and stops when `dxgkrnl` disables vsync interrupts; record what it found here

**Checkpoint**: commit.

## Phase 5 (US4): several outputs, and display power

- [ ] T018 [US4] Tests then the live output count in src/core/display/display_sources.{hpp,cpp}, read from virtio-gpu's `num_scanouts` with the array bound staying a compile-time maximum; a device reporting more than the maximum is clamped and the value logged
- [ ] T019 [US4] Child enumeration, the VidPn classes and `DisplayScanout` follow the live count, in src/core/adapter/adapter.cpp and src/core/vidpn/; L1 and L2 tests with two outputs
- [ ] T020 [US4] Runner: a `max_outputs=2` scenario where each output has its own scanout and mode, in tools/guest/run_guest_suite.py
- [ ] T021 [US4] A guest run with two outputs: the desktop extends onto both, display power off and on resumes scanout at the same mode, and the host disabling one output drops it without a TDR; record what it found here

**Checkpoint**: commit.

## Phase 6: What the feature promised

- [ ] T022 Rotation, scaling, gamma and multi-plane overlay asserted refused, not ignored, in L1 tests (FR-006, constitution VI)
- [ ] T023 Five consecutive guest runs reach the desktop (SC-001) with no Verifier violation or display TDR (SC-005); `AV_GUEST_IMAGE=... tools/gate.sh` passes; docs/testing.md gains the screendump and mode-switch scenarios; merge; push
