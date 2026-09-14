---

description: "Tasks: real-guest power, restart and recovery"
---

# Tasks: Real-guest power, restart and recovery

**Input**: Design documents from `/specs/040-guest-power-and-recovery/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/ddi_sequences.md, quickstart.md

**Tests**: Required, and written first. Constitution IV.

**Organization**: by user story, in priority order. Each `**Checkpoint**` is a commit once `ctest --test-dir build` passes there.

## Format: `[ID] [P?] [Story] Description`

- **[Story]**: US1 (sleep and hibernate), US2 (disable, enable, upgrade), US3 (a stalled context), US4 (a faulting executor)

---

## Phase 1: Foundational — the engine tells the truth about itself

**Purpose**: the defect the research found, fixed before anything is built on it: `QueryEngineStatus` answers "responsive" unconditionally, and `VidSchiCheckNodeTimeout` takes that as "no timeout this round". A hung host is currently deferred forever.

- [X] T001 Write `tests/unit/core/scheduling/test_engine_progress.cpp` first: an idle node is responsive; a node with work queued whose `progress_time()` moved since the last answer is responsive; one that has not moved is not; the very first answer is responsive whatever the node says (the check that asks has just decided the node is late, and one deferral is what a slow host may already be inside); two answers in the same tick cannot both be "moved"; readable at `Irql::dispatch` with no allocation, no block and no mutex
- [X] T002 Implement `EngineProgress` (object class) in `src/core/scheduling/engine_progress.{hpp,cpp}`
- [X] T003 `Adapter::query_engine_status` answers from it in `src/core/adapter/adapter.{hpp,cpp}`, and `tests/unit/core/adapter/test_adapter.cpp` asserts a node that stopped moving is reported unresponsive - citing `VidSchiCheckNodeTimeout` (dxgmms2.sys @1400c9b50) for why that matters

**Checkpoint**: a hung engine is admitted to dxgkrnl. Commit.

---

## Phase 2: User Story 1 - Sleep and hibernate (Priority: P1) 🎯 MVP

**Goal**: D3 and back to D0 leaves the adapter as power found it, and anything that cannot be restored is reported lost rather than used.

**Independent Test**: the L2 power sequence in `contracts/ddi_sequences.md`, then the guest's own S3 and S4 scenarios.

- [X] T004 [P] [US1] Write `tests/unit/core/adapter/test_power_transition.cpp` first: a transition holds what was running (device started, which sources were scanning out); `release()` answers it once and forgets it; a second `hold` without a release is `invalid_state`; a transition that held nothing restores nothing
- [X] T005 [US1] Implement `PowerTransition` (object class) in `src/core/adapter/power_transition.{hpp,cpp}`
- [X] T006 [US1] `Adapter::set_power_state` holds on the way down and restores on the way up in `src/core/adapter/adapter.cpp`: the device is started again, each source that was scanning out is configured again, and a failure to restore leaves the adapter running without that source rather than failing the transition
- [X] T007 [US1] Every DDI that touches the device refuses while power is down, tested in `tests/unit/core/adapter/test_adapter.cpp`: a flip, a cursor update, a scan-line query and a present answer `invalid_state` between D3 and D0 instead of touching a destroyed device
- [X] T008 [US1] The L2 suspend/resume scenario in `tests/unit/scenarios/test_ddi_scenarios.cpp`, driving the real table through the sequence the contract names
- [X] T009 [US1] The runner's `suspend` and `hibernate` scenarios in `tools/guest/run_guest_suite.py` (agent `guest-suspend-ram`, QMP `system_wakeup`; hibernation enabled inside the overlay for `guest-suspend-disk`, then the overlay booted again), each reported skipped with its reason where the machine does not offer the transition, with fake-guest tests in `tests/tools/test_run_guest_suite.py`
- [X] T010 [US1] Five S4 resumes with a passing `rvktest.exe` (SC-001), and S3 likewise where offered; record what the runs showed here

  **Five S4 resumes, each with the whole in-guest suite passing after it.**
  `--hibernate 5`: the guest suspended to disk, QEMU exited, the overlay booted
  again on the same configuration and the adapter came back every time -
  `hibernate ok: the adapter came back from S4, 1 of 5` through `5 of 5`, with
  `report ok: 29 passed` after each resume.

  Two measuring mistakes came out of those runs, both in what the run reads
  rather than in what the driver does, and both fixed:

  - the sleep states were read once and cached, and the first read happens
    before the stage that turns hibernation on, so a machine that had just
    gained S4 was reported as offering none;
  - the driver's rate window counted *reported* blanks, which dxgkrnl turns off
    the moment nothing is waiting, so five resumes with an idle desktop ended
    with "2.30 vertical blanks a second" for a timer that never left 60 Hz. The
    window now counts the timer's own periods and says how many were reported;
    the live guest then measures 58.75.

  **S3 on this machine cannot resume, and it is the firmware, not the driver.**
  The first live S3 bugchecked the guest. The driver's own log said where it was
  not: `SetPowerState D3 done status 0`, and then no D0 at all - the machine
  died between entering S3 and being powered back up. Two real driver defects
  came out of chasing it, both fixed and both tested:

  - the synthetic vertical blank kept its timer across D3 and reported a blank
    to an adapter dxgkrnl had not powered up yet, which Windows bugchecks for;
  - the ISR and the DPC checked only that the device object existed, so an
    interrupt arriving while `power_up` was between constructing and starting it
    harvested from queues that were not there.

  With both fixed the driver completes D0 cleanly - `SetPowerState D0 device`,
  `vsync`, `done`, all status 0 - and the guest agent answers again. The guest
  then still bugchecks, and the minidump names it: **0x5C
  HAL_INITIALIZATION_FAILED (0x201, ..., 0xC0000001)**, which is the guest's own
  HAL failing to come back, not a display driver. OVMF on q35 advertises S3
  through ACPI and cannot resume from it.

  So the machine stops advertising what it cannot do: the runner passes
  `-global ICH9-LPC.disable_s3=1` unless `--s3` says otherwise, Windows then
  reports `standby=no`, and the scenario skips with that reason instead of
  crashing the guest. SC-001's S3 half is "where offered", and it is not offered
  here; S4 is what this machine can actually do, and what the rest of this task
  measures.

**Checkpoint**: the guest sleeps and wakes with the adapter running. Commit.

---

## Phase 3: User Story 3 - A stalled host context is recovered alone (Priority: P1)

**Goal**: a context whose host work stalls is lost alone; the other process keeps completing.

**Independent Test**: the L2 timeout sequence, an L4 run with a stalling executor, and the L5 `tdr` scenario.

- [X] T011 [P] [US3] Write `tests/unit/core/scheduling/test_context_loss.cpp` first: a loss is recorded and logged once with the value that caused it; the same context lost twice is recorded once and logged once; `pending()` counts what the owner has not been told; `report()` tells dxgkrnl and counts what it told; recording is safe at `Irql::dispatch`
- [X] T012 [US3] Implement `ContextLoss` (system class) in `src/core/scheduling/context_loss.{hpp,cpp}` *Deleted in 044: it reported `DMA_FAULTED`, which dxgmms2.sys `VidSchDdiNotifyInterruptWorker` @140031b80 bugchecks 0x119/0xD on `DXGK_INTERRUPT_DMA_FAULTED` from a GPU MMU adapter.*
- [X] T013 [US3] `Adapter::reset_engine` records the contexts whose work it aborted as lost, and reports them, in `src/core/adapter/adapter.cpp`; `tests/unit/core/adapter/test_adapter.cpp` asserts the other device's context is untouched
- [X] T014 [US3] The L2 timeout and reset scenario in `tests/unit/scenarios/test_ddi_scenarios.cpp`: `QueryEngineStatus` deferring once, then refusing, then `ResetEngine` aborting only that node - and the driver returning at the IRQL it was called at, which `DdiResetEngine` (dxgkrnl.sys @14006e0a4) checks
- [X] T015 [US3] The echo executor gains stall and fault controls read from its environment at load, in `tests/qemu/echo_executor.cpp`, with the L4 loopback run that uses them
- [X] T016 [US3] The runner's `tdr` scenario: one context stalled past the timeout, the other still completing, event 4101 in the System log, the live kernel report collected; with its fake-guest test

  Built, and it runs: `--stall-ms` (and `--fault-after`) aim the host's stall or
  fault at one context (`RVK_ECHO_ONLY_NTH`, `--stall-context`), the guest is
  asked for `rvktest.exe tdr` - `TimeoutScenario`, two devices with a context
  each, both submitting - the `timeouts` provisioning stage reads event 4101 out
  of the System log, and a TDR live dump is expected of such a run and of no
  other.

- [~] T017 [US3] An injected 5 s stall recovers within 10 s with the other context unaffected (SC-003); record what the run showed here

  **What the first live run showed: the injection cannot reach a guest
  submission yet, and the reason is the roadmap, not this feature.** The run did
  everything it was asked to; the guest's verdict was

  ```
  ok timeout.held_submit
  ok timeout.other_submit
  FAIL timeout.held_lost status=0 code=0x00000000
  ok timeout.other_completed
  ```

  and the driver's log names why: `CreateDevice`, `CreateAllocation`, and no
  context or batch reaching the device at all. A guest submission today is
  fences kept by `ExecutionNode` and completed from the driver's own DPC; the
  path that carries a context's batches to the host executor is feature 043
  (guest transport) on top of 042 (the host executor), and 044 is where the two
  meet in a real guest. Until then a host that holds a context holds nothing the
  guest is waiting for, so dxgkrnl has nothing to time out.

  What is proven meanwhile is the whole path either side of that gap: L2 drives
  dxgkrnl's real timeout sequence through the driver's table (T014), L4 drives a
  stalling and a faulting executor through the device and asserts the faulted
  context is lost alone while the other completes (T015, T022), and L3 proves
  the injection reaches one executor and not the others. The L5 half of SC-003
  belongs to feature 044 and is recorded there.

**Checkpoint**: one context's stall is one context's problem. Commit.

---

## Phase 4: User Story 2 - Disable, enable and upgrade without reboot (Priority: P1)

**Goal**: fifty disable/enable cycles and a driver upgrade, with nothing left behind.

**Independent Test**: the L2 stop/start sequence fifty times, and the guest's `cycle` and `upgrade` scenarios.

- [X] T018 [US2] The L2 scenario in `tests/unit/scenarios/test_ddi_scenarios.cpp`: fifty StopDevice/StartDevice cycles on one adapter, with the mock platform's allocation balance and handle counts asserted after the last
- [X] T019 [US2] The runner's `cycle` scenario in `tools/guest/run_guest_suite.py` (`pnputil /disable-device` and `/enable-device`, `--power-cycles N`), the adapter started after each, Driver Verifier's pool tracking read after the last; with its fake-guest test and the `provision.ps1` stage it needs
- [X] T020 [US2] The runner's `upgrade` scenario: a package with a higher version installed over the running one with no reboot, and the adapter started on the new version
- [X] T021 [US2] Fifty cycles on the real guest: **done, and clean**. `cycle: 50 done`, and Driver Verifier after the last one: 459 pool allocations attempted, 459 succeeded, **current 8, peak 8** - the driver holds the same eight allocations after fifty disable/enable cycles as after one, and no bugcheck. Five upgrades have since run on the same guest, each installed over the
  running driver with no reboot and the adapter started on the new version:
  `0.35.1.0`, `0.35.3.0`, `0.35.6.0`, `0.35.10.0`, `0.35.15.0`, and then
  `report ok: 29 passed` and `vsync ok: 58.75 blanks a second`.

  The run also found the runner reading the guest's own output wrongly: the guest ends its lines the Windows way, and a regex anchored at `$` without allowing `\r` never matched `cycle: 50 done`. Every anchored pattern that reads guest output now tolerates it

**Checkpoint**: the driver survives being turned off and replaced. Commit.

---

## Phase 5: User Story 4 - A faulting executor (Priority: P2)

**Goal**: a fault loses the faulted context and nothing else.

- [X] T022 [US4] The L4 scenario with a faulting executor in `tests/loopback/`: the faulted context is reported lost to its owner, the other completes, and the device holds no leftover context or blob (FR-004)
- [~] T023 [US4] The runner's fault injection, alongside the stall, and the L5 run showing only the faulted context lost (SC-004); record what the run showed here

  The injection is built and reaches the device (`--fault-after`, aimed with
  `--stall-context`), and the `tdr` run carries it. Its L5 verdict waits on the
  same gap as T017: a fault on the host's side of a context is invisible to a
  guest whose submissions never leave the driver. SC-004 is proven at L4 today
  (`tests/loopback/test_loopback_context.cpp`: the faulted context reports its
  code while another completes and sets up again) and is recorded for L5 with
  feature 044.

**Checkpoint**: a fault is not an outage. Commit.

---

## Phase 6: What the feature promised

- [X] T024 A device that changed across hibernation - a different host GPU UUID, an older protocol version - is refused with its contexts reported lost and the desktop still working (FR-005), tested at L2 and L4

  `DeviceIdentity` (object class) keeps the capset the adapter started on;
  `CapsetExchange` (object class) reads one over the control queue, which the
  adapter now does at StartDevice and again at D0. Another host GPU UUID, an
  older protocol minor or another major is another device: its queued work is
  reported faulted, a new context is refused, a submission on a context that
  survived is answered with a fault, and the transition still succeeds with the
  scanout restored - not bugchecking is what FR-005 asks for. L2 drives that
  through the real table; L4 shows two device instances bound to different host
  GPUs answering for themselves, with the identity refusing the second.
- [X] T025 `docs/kb/05-scheduling.md` gains what the timeout path actually asks for, and `docs/testing.md` the new L5 scenarios
- [X] T026 `AV_GUEST_IMAGE=... tools/gate.sh` passes; merge to `master`; push

  Every configuration passed - default, coverage (100% branches), fuzz, qemu -
  and the live guest run with them: 29 steps passed, the display steps clean,
  and `vsync ok: 58.73 blanks a second` from the window that now counts the
  driver's own timer periods. The real device answers the capset the adapter
  reads at StartDevice: `rvkgpu: device capset status 0x00000000`.

---

## Dependencies & Execution Order

- **Phase 1** is foundational: US3's whole story is built on the engine telling
  the truth, and US1's resume path reports losses through the same class.
- **US1 (Phase 2)** and **US3 (Phase 3)** are independent of each other once
  Phase 1 lands; US1 first, because sleep is what a laptop does hourly and a
  TDR is what a bug does.
- **US2 (Phase 4)** depends on nothing but the existing StopDevice path.
- **US4 (Phase 5)** uses US3's executor controls.
- **Phase 6** is everything.

### Parallel opportunities

- T004 and T011 are two test files ahead of two classes.
- The runner scenarios (T009, T016, T019) are separate steps in the same file;
  their fake-guest tests are independent.

---

## Implementation Strategy

**MVP**: Phases 1 and 2 - an engine that admits it is stuck, and a guest that
sleeps and wakes with its adapter. **Increment 2**: Phase 3, the part that keeps
one process's stall from taking the desktop down. **Increment 3**: Phases 4 and
5.

---

## Notes

- Nothing on the TDR path may allocate, block or take a mutex; `MockPlatform`
  enforces it and the tests drive that path at `Irql::dispatch`.
- The driver must return at the IRQL it was called at: both DDIs in the
  contract check it, and a checked build bugchecks 0x113 when it does not.
- A loss is logged once, with the value that caused it (FR-007).
