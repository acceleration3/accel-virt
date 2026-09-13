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
- [ ] T010 [US1] Five S4 resumes with a passing `rvktest.exe` (SC-001), and S3 likewise where offered; record what the runs showed here

**Checkpoint**: the guest sleeps and wakes with the adapter running. Commit.

---

## Phase 3: User Story 3 - A stalled host context is recovered alone (Priority: P1)

**Goal**: a context whose host work stalls is lost alone; the other process keeps completing.

**Independent Test**: the L2 timeout sequence, an L4 run with a stalling executor, and the L5 `tdr` scenario.

- [X] T011 [P] [US3] Write `tests/unit/core/scheduling/test_context_loss.cpp` first: a loss is recorded and logged once with the value that caused it; the same context lost twice is recorded once and logged once; `pending()` counts what the owner has not been told; `report()` tells dxgkrnl and counts what it told; recording is safe at `Irql::dispatch`
- [X] T012 [US3] Implement `ContextLoss` (system class) in `src/core/scheduling/context_loss.{hpp,cpp}`
- [ ] T013 [US3] `Adapter::reset_engine` records the contexts whose work it aborted as lost, and reports them, in `src/core/adapter/adapter.cpp`; `tests/unit/core/adapter/test_adapter.cpp` asserts the other device's context is untouched
- [ ] T014 [US3] The L2 timeout and reset scenario in `tests/unit/scenarios/test_ddi_scenarios.cpp`: `QueryEngineStatus` deferring once, then refusing, then `ResetEngine` aborting only that node - and the driver returning at the IRQL it was called at, which `DdiResetEngine` (dxgkrnl.sys @14006e0a4) checks
- [ ] T015 [US3] The echo executor gains stall and fault controls read from its environment at load, in `tests/qemu/echo_executor.cpp`, with the L4 loopback run that uses them
- [ ] T016 [US3] The runner's `tdr` scenario: one context stalled past the timeout, the other still completing, event 4101 in the System log, the live kernel report collected; with its fake-guest test
- [ ] T017 [US3] An injected 5 s stall recovers within 10 s with the other context unaffected (SC-003); record what the run showed here

**Checkpoint**: one context's stall is one context's problem. Commit.

---

## Phase 4: User Story 2 - Disable, enable and upgrade without reboot (Priority: P1)

**Goal**: fifty disable/enable cycles and a driver upgrade, with nothing left behind.

**Independent Test**: the L2 stop/start sequence fifty times, and the guest's `cycle` and `upgrade` scenarios.

- [X] T018 [US2] The L2 scenario in `tests/unit/scenarios/test_ddi_scenarios.cpp`: fifty StopDevice/StartDevice cycles on one adapter, with the mock platform's allocation balance and handle counts asserted after the last
- [ ] T019 [US2] The runner's `cycle` scenario in `tools/guest/run_guest_suite.py` (`pnputil /disable-device` and `/enable-device`, `--power-cycles N`), the adapter started after each, Driver Verifier's pool tracking read after the last; with its fake-guest test and the `provision.ps1` stage it needs
- [ ] T020 [US2] The runner's `upgrade` scenario: a package with a higher version installed over the running one with no reboot, and the adapter started on the new version
- [ ] T021 [US2] Fifty cycles and five upgrades on the real guest, no leak and no bugcheck (SC-002); record what the runs showed here

**Checkpoint**: the driver survives being turned off and replaced. Commit.

---

## Phase 5: User Story 4 - A faulting executor (Priority: P2)

**Goal**: a fault loses the faulted context and nothing else.

- [ ] T022 [US4] The L4 scenario with a faulting executor in `tests/loopback/`: the faulted context is reported lost to its owner, the other completes, and the device holds no leftover context or blob (FR-004)
- [ ] T023 [US4] The runner's fault injection, alongside the stall, and the L5 run showing only the faulted context lost (SC-004); record what the run showed here

**Checkpoint**: a fault is not an outage. Commit.

---

## Phase 6: What the feature promised

- [ ] T024 A device that changed across hibernation - a different host GPU UUID, an older protocol version - is refused with its contexts reported lost and the desktop still working (FR-005), tested at L2 and L4
- [ ] T025 `docs/kb/05-scheduling.md` gains what the timeout path actually asks for, and `docs/testing.md` the new L5 scenarios
- [ ] T026 `AV_GUEST_IMAGE=... tools/gate.sh` passes; merge to `master`; push

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
