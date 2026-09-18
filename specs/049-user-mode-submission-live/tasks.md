---
description: "Tasks: user-mode submission in the guest (049)"
---

# Tasks: user-mode submission in the guest

**Input**: plan.md, spec.md, research.md

**Tests**: written first (constitution IV); every branch, invariants after every
mutation, and a hostile table for everything a process writes - the ring tail,
the fence value it claims, a store to a doorbell it disconnected (FR-005).
Commit at every **Checkpoint** once `ctest --test-dir build -LE tools -j6`
passes.

**What this feature does not build: native fences.** FR-003 and research.md §9
say why - thirteen further DDIs with GPU-visible fence memory and log buffers,
which is a feature's work beside the doorbell's. The wait stays
`Kmt::wait_for_context`.

## Phase 1: Foundational - the doorbell DDIs

- [X] T001 The four `*Record` structures in `src/core/ddi/`: `DxgkargCreateDoorbellRecord`, `DxgkargConnectDoorbellRecord`, `DxgkargDisconnectDoorbellRecord`, `DxgkargDestroyDoorbellRecord` and `DxgkargNotifyWorkSubmissionRecord`, with the WDK's own field names (`d3dkmddi.h:10270-10390`) and every offset proven in `src/km/ddi_record_layout.cpp`
- [X] T002 `Doorbell` (object) in `src/core/scheduling/doorbell.{hpp,cpp}`: one process's doorbell - the hardware queue it belongs to, the doorbell page index its context was bound to, whether it is connected, and the `D3DDDI_DOORBELLSTATUS` a process reads. Tests `tests/unit/core/scheduling/test_doorbell.cpp`
- [X] T003 `DoorbellTable` (system) in `src/core/scheduling/doorbell_table.{hpp,cpp}`: the adapter's doorbells by handle, with a fixed maximum (`RingSubmitter::doorbell_pages`, 64), created on a queue and forgotten with it. Tests `tests/unit/core/scheduling/test_doorbell_table.cpp`: a handle nobody made, a queue destroyed under a live doorbell, a table with no room, and every clause of the invariant
- [X] T004 The five one-line entry points in `src/core/ddi/entry_points.cpp` and their slots (166-170) in `src/core/miniport/miniport.cpp`, forwarding to `Adapter::create_doorbell`, `connect_doorbell`, `disconnect_doorbell`, `destroy_doorbell`, `notify_work_submission`
- [X] T005 `Adapter::query_adapter_info` answers `DXGKQAITYPE_USERMODESUBMISSION_CAPS` (38) with `DXGK_USERMODESUBMISSION_CAPS{DoorbellSizeInBytes = 4}` and no secondary doorbell - the page is a 4-byte store at offset 0 (`rvk_device.h`), and a capability is reported only once the DDIs behind it are implemented (constitution VI). Tests in `tests/unit/core/adapter/`
- [X] T006 Disconnect makes later stores harmless **before it returns** (FR-002, docs/kb/05-scheduling.md). **No dummy page**, and research.md §10 says why: a store here signals an eventfd and the worker reads the ring's tail, which is what says there is work - so a store to a disconnected doorbell costs one spurious wake and does nothing. What `disconnect_doorbell` does is mark the doorbell disconnected, answer `DISCONNECTED_RETRY` as its status, and refuse a kernel address for it until it reconnects. Tests: the status a process would read, a connect after a disconnect, and a store whose tail did not move consuming nothing (over the loopback rig)

**Checkpoint**: commit. The DDIs exist and the conformance harness drives them.

## Phase 2: US1 - submission without a kernel transition (P1)

- [ ] T007 [US1] `Kmt` gains `create_doorbell`, `connect_doorbell`, `destroy_doorbell` and `notify_work_submission` (`d3dkmthk.h:886-945`), implemented in `src/um/gdi_kmt.cpp` over the real D3DKMT entry points and in `MockKmt` for the tests
- [X] T008 [US1] `GuestDoorbell` (object) in `src/guest/transport/guest_doorbell.{hpp,cpp}`: the three mappings a create answers - the doorbell, the status page, and where the queued fence value is written - and the store. Tests `tests/unit/guest/transport/test_guest_doorbell.cpp` over MockKmt: the store lands at offset 0 of the page, the status is read before every store, and nothing is stored while disconnected.
      **What this moves across the kernel boundary**: today the process owns the tail counter and *the miniport publishes it in the ring header* once it has checked the submission (`src/guest/transport/transport_ring.hpp:1-9`, docs/protocol.md Submission). A process that stores to the doorbell itself must publish that tail itself, so `TransportRing` gains a `publish_tail()` the doorbell path calls and the kernel path does not - and the device's existing validation of that tail (`RingConsumer`, feature 030) becomes the only thing standing between a hostile process and the ring, which is what T014 tests
- [X] T009 [US1] `SubmissionPath` (object) in `src/guest/transport/submission_path.{hpp,cpp}`: which way one submission goes, by the status page alone - `CONNECTED` stores, `CONNECTED_NOTIFY_KMD` stores and then calls, `DISCONNECTED_RETRY` and `DISCONNECTED_ABORT` submit the old way. Tests: every status, and that the connected path makes no Kmt call at all (FR-004)
- [X] T010 [US1] `KmtTransport` submits through `SubmissionPath` instead of calling `kmt().submit()` directly, and the adapter that reports no user-mode submission keeps the old path with identical behaviour (spec.md edge case). Tests in `tests/unit/guest/transport/`

**Checkpoint**: commit.

## Phase 3: US2 - disconnection is survived (P1)

- [X] T011 [US2] `MockKmt` scripts a disconnect mid-stream: the status page it hands out changes under the transport, as dxgkrnl's does. Tests: the store after it is not made, the submission goes through `D3DKMTSubmitCommand`, and the transport reconnects when the status says connected again
- [X] T012 [US2] No record is executed twice and none is lost across a disconnect (US2 scenario 2): the ring's tail is published once and the fallback submits the same tail, so a resubmission the device already consumed is a tail it has already passed. Tests over the loopback rig: a disconnect between the ring write and the store, and one between the store and the completion
- [X] T013 [US2] The full-stack rig shows both halves together: a user-mode store reaches the device worker with no `SubmitCommand`, a disconnect makes the next store harmless, and the fallback's work completes. `tests/loopback/test_fullstack_doorbell.cpp`
- [X] T014 [US2] Hostile input (FR-005, constitution V): a store of an arbitrary value, a store to a disconnected doorbell, a ring tail past the ring, and a fence value past what was queued - each faults that context and no other, and `RingConsumer`'s existing tail validation is shown to hold for a tail user mode wrote

**Checkpoint**: commit.

## Phase 4: US3 - the gain is measured and held (P2)

- [X] T015 [US3] `SubmitBenchmark` (object) in `src/guest/suite/submit_benchmark.{hpp,cpp}`: a scripted number of no-op submissions each way on one boot, answering both per-submit costs. Tests over a scripted Kmt
- [X] T016 [US3] The in-guest suite reports both costs and their ratio (`ok submit.doorbell_ns`, `ok submit.kernel_ns`, `ok submit.ratio`), and `tools/guest/run_guest_suite.py` records the ratio against `tools/submit_ratchet.txt` - the absolute cost is the host's, the ratio is the driver's (research.md §5). Tests in `tests/tools/test_run_guest_suite.py`
- [X] T017 [US3] The ratchet file is committed **empty** until a guest run fills it, as 048's conformance ratchet is, and the runner says so rather than passing quietly

**Checkpoint**: commit.

## Phase 5: Polish and merge

- [X] T018 docs: `docs/testing.md` (what the doorbell path is and how the rig drives it), `docs/kb/05-scheduling.md` (what this driver answers for the doorbell DDIs, and that native fences are not here)
- [ ] T019 Coverage and fuzz smoke; `AV_GATE_JOBS=6 AV_GUEST_IMAGE=... tools/gate.sh`; merge; push

## Dependencies

- Phase 1 blocks everything: nothing in the guest can create a doorbell until
  the DDIs answer.
- T006 blocks T011 (the disconnect the transport survives is the one the KMD
  performs), and T009 blocks T010.
- Phase 4 needs Phase 2 for the doorbell path to measure.

## Implementation strategy

The MVP is Phases 1-2: a submission that reaches the device with no kernel
transition. Phase 3 is what makes it safe to leave on, and Phase 4 is what
proves it was worth doing.
