---
description: "Task list for feature 038: real-guest desktop"
---

# Tasks: Real-guest desktop: DWM on rvkgpu at a chosen mode

Tests come first in every task and must fail before the code they cover exists.
Each guest run records what it found in this file; every disagreement is
reproduced at L2, citing the `dxgkrnl.sys` or QEMU function the belief was read
from, before it is fixed (FR-007).

## Phase 1: The wire requests the 2D path needs

- [X] T001 [P] Tests then `set_scanout` in src/proto/virtio_gpu/gpu_request_writer.{hpp,cpp}: type `0x0103`, the scanout id, resource id and rectangle it writes, and a buffer one byte too small refused with nothing written
- [X] T002 [P] Tests then `resource_flush` in src/proto/virtio_gpu/gpu_request_writer.{hpp,cpp}: type `0x0104`, the resource id and rectangle, and a short buffer refused
- [X] T003 [P] Tests then `get_display_info` in src/proto/virtio_gpu/gpu_request_writer.{hpp,cpp}: type `0x0100` with a header and no payload, and a short buffer refused
- [X] T004 Runner: a QMP `screendump` into the work directory and a comparison answering whether two dumps differ, in tools/guest/run_guest_suite.py; tests over the fake QEMU in tests/tools/fake_guest/

**Checkpoint**: commit.

## Phase 2 (US1): the committed frame reaches the host

- [X] T005 [US1] Tests then `ScanoutResource` in src/core/display/scanout_resource.{hpp,cpp}: `CREATE_2D` and `ATTACH_BACKING` once per mode, `SET_SCANOUT` once per backing, `TRANSFER_TO_HOST_2D` plus `RESOURCE_FLUSH` only when the address the host was told differs from the recorded one, `invariants_hold` after every mutation, `Irql::dispatch` ceiling on the flush path
- [X] T006 [US1] Tests then `DisplayScanout` in src/core/display/display_scanout.{hpp,cpp}: one `ScanoutResource` per source, distinct resource ids, a source out of range refused, teardown releasing every resource
- [X] T007 [US1] `DisplayScanout` wired into src/core/adapter/adapter.cpp: `commit_vidpn` configures the committed source, `dpc_routine` flushes a source the host is behind on, `stop_device` tears the resources down; L1 tests over the fake virtio-gpu function assert the requests and their order
- [X] T008 [US1] An L2 scenario in tests/unit/scenarios/test_ddi_scenarios.cpp: commit a VidPn, set the source address, run the DPC, and assert the device saw `CREATE_2D`, `ATTACH_BACKING`, `SET_SCANOUT`, `TRANSFER_TO_HOST_2D` and `RESOURCE_FLUSH` for that source
- [X] T009 [US1] A guest run: screendumps before and after the guest opens a window differ, `dwm.exe` runs, the adapter is `rvkgpu` with status OK, Basic Display is not bound, and no display TDR is logged; record what it found here

  **What the first run found.** Three of the four hold on the real guest:
  `dwm=1`, `services=rvkgpu` - nothing else is bound to a display - status OK,
  problem 0, and no display TDR. QMP `screendump` answers a 1280x800 PPM where
  before this feature it answered nothing.

  No frame reaches the host. The dump holds two colours - 1,023,316 black pixels
  and 684 grey - which is QEMU's own "display output is not active" text, not
  our scanout: T009a proved it by reading the same two counts again from a run
  whose `SET_SCANOUT` never went out. The cause is in the driver:
  `PagingRequest::build` answers
  `DXGK_OPERATION_MAP_APERTURE_SEGMENT` with `Status::ok` and records nothing,
  so the only address the scanout has is the one `SetVidPnSourceAddress` gives -
  an aperture segment address, `ApertureSegment::base_address + offset` - and
  `ATTACH_BACKING` needs guest physical pages. The host is reading whatever sits
  at 0xC0000000 + offset, which is mostly zeros.

  The window-and-cursor half of the acceptance cannot be tested the way it is
  written: the guest agent is a session 0 service and cannot open a window on
  the interactive desktop. The step asserts what an arriving frame proves
  instead - the committed size, and that the dump is not QEMU's placeholder -
  and US2's mode switch covers "the dump follows what the guest did". T009a
  says what the content of the frame waits on.

- [X] T009a [US1] The aperture the primary really lives in: `map_aperture_segment` and `unmap_aperture_segment` record the pages dxgkrnl gives for each segment offset, in src/core/memory/; `ScanoutResource` attaches those pages, with `ATTACH_BACKING` carrying one memory entry per run of contiguous pages; hostile-input tests for the operation's MDL and a fuzz target

  **What it took.** `GuestPageList` (object class, src/proto/resources) is a view
  over page frame addresses that knows how they group into runs;
  `AperturePages` (object class, src/core/memory) is the window of frames one
  allocation is mapped into the aperture with, owned by `Allocation` and filled
  from the operation's MDL through a new `Platform::read_page_frames`;
  `GpuRequestWriter::attach_backing_pages` writes one memory entry per run;
  `DisplaySources` records the primary's handle beside its address and forgets
  it when the allocation is destroyed, and `DisplayScanout` asks that
  allocation for the pages of the address it is about to attach. Each of the
  three guest runs below ended in one defect, each in our own driver.

  **10th defect: a primary VidMm never pins.** Without
  `DXGK_ALLOCATIONINFOFLAGS.AccessedPhysically` (0x8000) VidMm leaves a primary
  in its own implicit system memory segment and `DxgkDdiSetVidPnSourceAddress`
  answers `PrimarySegment` 0, `PrimaryAddress` 0 - an address with no pages
  behind it, and no `DXGK_OPERATION_MAP_APERTURE_SEGMENT` ever arrives. Three
  consecutive runs logged `rvkgpu: SetVidPnSourceAddress source 0 segment 0
  address 0x0000000000000000`, including one with the primary reported as not
  CPU visible and one with the aperture as its only supported segment set: the
  placement does not follow `PreferredSegment`. With the flag set the same line
  reads `segment 1 address 0x00000000C0000000` and the mapping arrives with a
  1000-page MDL. The display has no GPU MMU to read through, so an allocation
  it scans out has to be pinned in a physical segment, and that flag is how a
  WDDM 2.0 GPU-MMU driver says which allocations those are.

  **11th defect: a request wider than a queue slot.** `GpuCommandQueue::send`
  refused anything longer than `GpuRequestWriter::max_request_size` (64 bytes)
  with `invalid_parameter`, and the backing of a 1280x800 primary is 12 memory
  entries - 224 bytes. The driver said so itself:
  `rvkgpu: scanout 0 address 0x00000000C0000000 pages 1000 runs 12 needs 1000
  status 1`. The control queue now has one *wide* slot beside its 16 narrow
  ones, sized for one memory entry per page of the largest mode the driver
  offers; a narrow request never takes it, so a burst of small requests cannot
  keep a primary off the screen.

  **12th defect: a frame that could not be carried said nothing.** The DPC
  logged only through `LogBudget`, which takes a spin lock and so cannot be
  reached from `SetVidPnSourceAddress` at device IRQL, and the scanout path
  logged nothing at all. `ScanoutResource::report` now explains, once per
  address, any frame whose pages do not cover it or that the host refused, and
  `Adapter::log_scanout` says where dxgkrnl points a source whenever the IRQL
  allows the budget's lock. Every finding above was read out of those two lines.

  **What the run proves now, and what it cannot.** The host scans out the
  guest's own primary: the screendump is 1280x800, QEMU's "display output is
  not active" placeholder is gone, the driver recorded 1000 aperture pages in 12
  runs and refused no frame. The picture is all zeros, because nothing writes
  the primary: DWM composites with the GPU, and no GPU work is executed until
  the host executor and the guest transport land (features 042-044). So US1's
  "the screendump changes when the guest's desktop does" belongs to 044, and
  the runner's `display` step asserts what an arriving frame proves today - the
  committed size, a scanout that is not QEMU's placeholder, the primary's pages
  recorded, and no refused frame.

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
