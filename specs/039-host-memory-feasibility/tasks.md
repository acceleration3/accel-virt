---

description: "Tasks: host memory and interrupt feasibility on real host GPUs"
---

# Tasks: Host memory and interrupt feasibility on real host GPUs

**Input**: Design documents from `/specs/039-host-memory-feasibility/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/host_ops.md, quickstart.md

**Tests**: Required. Constitution IV is non-negotiable: each test is written before the code it judges and must fail first.

**Organization**: by user story, in priority order. Each `**Checkpoint**` is a commit once `ctest --test-dir build` passes there.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: different files, no dependency on an unfinished task
- **[Story]**: US1 (host-visible memory), US2 (guest import), US3 (interrupt stress), US4 (capset bits)

---

## Phase 1: Setup

**Purpose**: the folder the probes live in, and the result type all three share.

- [X] T001 Write `tests/unit/device/probe/test_probe_result.cpp` first: an unanswered result is `not_run` and not `passed`; `record_pass(bytes)` then `passed()` with `measured()`; `record_failure(reason, host_error)` then `reason()` and `errno_value()`; a second answer of either kind is refused with `Status::invalid_state` and leaves the first; `describe(TextBuffer &)` names outcome and reason
- [X] T002 Implement `ProbeResult` (object class) in `src/device/probe/probe_result.{hpp,cpp}` with `Outcome{not_run, passed, failed}`, `Status reason_`, `int32_t errno_`, `uint64_t measured_`

**Checkpoint**: the shared result type exists and is exhaustively tested.

---

## Phase 2: Foundational (blocks every user story)

**Purpose**: the host operations the probes call, on both sides of the `Host` contract, per `contracts/host_ops.md`. Nothing in this phase decides anything - it only makes the calls available and refusable.

- [X] T003a Add `bind_gpu(uint32_t index)` to `av::Host`, the ABI table and `TableHost`, called by `GpuBinding::bind` once the instance's host GPU is resolved: the glue cannot allocate on a physical device nobody named it, and every later feature needs the same logical device
- [X] T003 Add `export_gpu_memory(uint64_t size, void *&pointer, int32_t &host_error)`, `release_gpu_memory(void *)`, `import_guest_memory(void *pointer, uint64_t size, int32_t &host_error)`, `release_guest_memory(void *)` and `read_imported_memory(void *memory, uint8_t *buffer, uint64_t size)` to `av::Host` in `src/device/host/host.hpp`, each documenting its side (main loop) and failure shape
- [X] T004 Extend the C ABI table with the five entries in the same order in `src/device/abi/rvk_device.h`, and forward them in `src/device/abi/table_host.{hpp,cpp}`
- [X] T005 [P] Script them in `tests/harness/host/mock_host.{hpp,cpp}`: success, refusal with a `VkResult`, refusal with an `errno`, a readback that differs from what was written, and the balance check that every handle is released before the test ends
- [X] T006 [P] Extend `tests/harness/host/mock_host_ops.{hpp,cpp}` so the ABI table test covers the five new entries
- [X] T007 Implement them over Vulkan in `src/device/qemu/virtio-gpu-rvk-host.c`: `export_gpu_memory` allocates on the bound physical device with `VK_KHR_external_memory_fd`, maps it, and answers null plus the `VkResult` or `errno` on any failure; `import_guest_memory` uses `vkGetMemoryHostPointerPropertiesEXT` and an imported allocation, answering null with `host_error` 0 when the extension is absent; `read_imported_memory` copies what the GPU sees. No abort, no assert, on any path (FR-004)
- [X] T008 Extend `tests/unit/device/abi/test_table_host.cpp` so a missing entry in the table is a failure, keeping the ABI's "every member is required" rule

**Checkpoint**: the host contract carries the probes' calls and every refusal is scriptable.

---

## Phase 3: User Story 1 - Host-visible memory in a KVM memslot (Priority: P1) 🎯 MVP

**Goal**: the device finds out, at realize, whether memory the host GPU exports reaches the guest through the hostmem BAR, and says so.

**Independent Test**: `tools/host/probe_host_memory.py --qemu build-qemu` prints a verdict per host GPU; with `--guest-check --kernel <image>` a minimal Linux guest reads and writes the mapping under KVM.

- [X] T009 [P] [US1] Write `tests/unit/device/probe/test_host_memory_probe.cpp` first: no hostmem region leaves the result `failed` with `not_supported` and asks the host for nothing; an export refusal records the `VkResult` it was given; a `place_in_hostmem` refusal records its status and still releases the export; a readback that differs is `Status::malformed`; a run that works is `passed()` with `measured()` equal to `probe_bytes`; every path leaves no block, no placement and no guest mapping behind; `run()` twice is `invalid_state`
- [X] T010 [US1] Implement `HostMemoryProbe` (object class) in `src/device/probe/host_memory_probe.{hpp,cpp}`: `probe_bytes = 0x100000`, placement at hostmem offset 0, pattern write, readback compare, `ScopeExit` cleanup, `require_main_loop`
> **Order note**: T011-T013 read what the device *says* at realize, and the
> realize wiring is T023. Doing them before it would mean writing that wiring
> twice, so they run after Phase 5; the probe itself (T009, T010) is what US1
> depends on and it is done.

- [X] T011 [US1] Write the failing test for the runner first in `tests/tools/test_probe_host_memory.py` (captured device output in, JSON out, a refused probe still exit 0), then `tools/host/probe_host_memory.py`: one device per host GPU from `list_gpus.py`'s enumeration, realize only, JSON per GPU with driver, version, outcome, host error and measurement
- [~] T012 [US1] The in-guest KVM check, **deferred to 041 with its reason recorded here**. A guest can only read the mapping while something is placed in the region, and the probe places its block inside realize and takes it away again - so a guest reading the BAR afterwards would find the hole, not the mapping. Keeping a block placed for a guest to find would be a test-only device mode (constitution VIII), and the only honest way to hold one is the path 041 builds: the guest asks for a blob and maps it. What the probe does prove today is the half that is this feature's question - the host driver's export `mmap`s, `MAP_FIXED` over the hostmem reservation carries the bytes, and the region reads back what the GPU wrote. The runner's `--guest-check` reports `skipped` with this reason rather than pretending. 041's first mapped blob under KVM is the check, and its spec carries the pointer
- [X] T013 [US1] Record this host's answers (Intel Mesa ANV, NVIDIA proprietary; llvmpipe and AMD as not measured) in `docs/kb/04-memory.md` with driver, version, outcome, errno and measurement (FR-002)

**Checkpoint**: US1 answered on this host, both halves, and recorded. Commit.

---

## Phase 4: User Story 2 - Guest-backed pages imported by the host driver (Priority: P1)

**Goal**: the device finds out whether the host GPU imports a window over guest pages, and measures the copy fallback when it does not.

**Independent Test**: the same runner reports the import verdict per GPU; with the extension refused, the fallback throughput is what it records.

- [X] T014 [P] [US2] Write `tests/unit/device/probe/test_guest_import_probe.cpp` first: nothing mapped at the address is `not_found`; a bounce buffer (`direct` false) is refused without asking the host to import; an import refusal with `host_error` 0 is a clean `failed` (no extension); a readback through the import that differs is `malformed`; success is `passed()` carrying the bytes; every path releases the import and the guest mapping; a second `run()` is `invalid_state`
- [X] T015 [US2] Implement `GuestImportProbe` (object class) in `src/device/probe/guest_import_probe.{hpp,cpp}`: `map_guest` of `probe_bytes` at the address it is given, `direct` required, import, pattern, `read_imported_memory` compare, `ScopeExit` cleanup, `require_main_loop`
- [X] T016 [US2] Measure the copy fallback: there is no copy path in the device yet - guest-backed blobs are mapped, and the copy belongs to 041 - so the probe measures the copy a fallback would have to do: `probe_bytes` from the mapped guest window into a host memory block, timed with a new `Host::monotonic_nanoseconds()`, recorded as bytes per second in `measured()` when the import was refused. Scripted clock in `MockHost`, `g_get_monotonic_time()` in the glue
- [X] T017 [US2] Extend `tools/host/probe_host_memory.py` and its test to report the import verdict and the fallback throughput per GPU, and record the answers in `docs/kb/04-memory.md`

**Checkpoint**: US2 answered on this host and recorded. Commit.

---

## Phase 5: User Story 4 - Every answer is a capset bit with a fallback (Priority: P1)

**Goal**: what the probes decided is what the guest is told, and nothing else.

**Independent Test**: L1 tests of every bit set and clear; the L4 loopback suite passes with each bit cleared.

- [X] T018 [P] [US4] Write `tests/unit/proto/context/test_capset_info.cpp` additions first: `feature_gpu_exported_memory` (bit 2) and `feature_guest_memory_import` (bit 4) round-trip through `encode`/`decode`, and an unknown bit still survives
- [X] T019 [US4] Add the two constants to `src/proto/context/capset_info.hpp`
- [X] T020 [P] [US4] Write `tests/unit/device/probe/test_host_feasibility.cpp` first: `decide()` runs both memory probes once; `features()` carries a bit only for a probe that passed; `feature_host_visible_memory` follows the hostmem region; deciding twice changes nothing; `interrupts()` hands out the same stress probe
- [X] T021 [US4] Implement `HostFeasibility` (system class) in `src/device/probe/host_feasibility.{hpp,cpp}`
- [X] T022 [US4] Make `DeviceCapset::describe` take its feature mask from `HostFeasibility` in `src/device/capset/device_capset.{hpp,cpp}`, and update `tests/unit/device/capset/test_device_capset.cpp` so no bit is set without a passed probe (constitution VI)
- [X] T023 [US4] Run the probes at realize in `src/device/module/virtio_gpu_rvk.{hpp,cpp}`, logging one line per probe with its outcome and reason; a failed probe never fails realize (FR-004), asserted in `tests/unit/device/module/test_virtio_gpu_rvk.cpp`
- [X] T024 [US4] Prove the fallbacks at L4: the loopback suite reads the capset through the guest driver and asserts the device offers exactly what its probes proved - the rig's host has a hostmem region (export passes) and nothing where the import probe looks (that bit clear) - and runs a context end to end on that mask. The *guest* fallback the spec asks for cannot be exercised yet: no guest component reads these bits until 041, which is where the copy path it chooses between is built

  **What the probes answered on the development host.** Run with a 256 MiB
  hostmem region and memfd-backed guest RAM, both GPUs pass both probes - but
  NVIDIA only after the export learned a second handle type. Its proprietary
  driver exports `VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT` memory that
  `mmap` refuses with EINVAL, which is exactly the risk kb 11's M2 spike list
  names; its `DMA_BUF` export maps, and the guest reads it. Intel's Mesa ANV
  takes either. The export now tries opaque fd, then dma-buf, and only then
  gives up.

  Two more things the live run found, both in the glue, both about realize
  being earlier than it looks: the device has no DMA address space until the
  bus plugs it in (mapping guest RAM through it segfaulted QEMU, so the probes
  map through the machine's own address space), and a realize now says one line
  per probe, which the smoke test had to learn is the device working rather
  than complaining.

**Checkpoint**: every bit has a probe behind it and a fallback under it. Commit.

---

## Phase 6: User Story 3 - Interrupt injection under stress (Priority: P2)

**Goal**: know that fence interrupts survive the guest masking and unmasking its interrupt, at the highest rate this host sustains.

**Independent Test**: a long guest run with the stress scenario; no interrupt while disabled, no lost final fence, and a recorded latency distribution.

- [X] T025 [P] [US3] Write `tests/unit/device/probe/test_interrupt_stress_probe.cpp` first: an interrupt raised between `disabled()` and `enabled()` is a failure; a final fence never acknowledged is a failure; acknowledgements carry the worst delay into `measured()`; counts survive raises from several workers (IRQL/threading rules enforced by `MockHost`)
- [X] T026 [US3] Implement `InterruptStressProbe` (object class) in `src/device/probe/interrupt_stress_probe.{hpp,cpp}`, with `raised()` on the worker side and the rest on the main loop
- [X] T027 [US3] Report into it from the fence path in `src/device/fence/fence_interrupts.{hpp,cpp}`, tested where the existing fence tests are
- [X] T028 [US3] Add the guest side: a stress scenario in `src/guest/suite/` that raises fences as fast as the host sustains while the miniport disables and re-enables its interrupt, with `--stress-minutes` in `tools/guest/run_guest_suite.py` and its fake-guest test
- [ ] T029 [US3] Run it for ten minutes against the real guest and record the rate, the worst delay and the verdict in `docs/kb/05-scheduling.md`

**Checkpoint**: US3 measured on the real guest and recorded. Commit.

---

## Phase 7: What the feature promised

- [X] T030 Every probe's hostile-input path is covered: a host that answers a handle with no mapping behind it (which the memory probe leaked until this test existed - the release is now armed the moment a handle exists), a region or an import read the host refuses outright, a guest window the host maps only part of, and a host error outside any range this code names
- [X] T031 `docs/kb/11-implementation-plan.md`'s M2 spike list points at the recorded answers, and `docs/testing.md` gains the probe runner and the stress scenario
- [ ] T032 `AV_GUEST_IMAGE=... tools/gate.sh` passes; merge to `master`; push

---

## Dependencies & Execution Order

- **Phase 1 (Setup)**: no dependencies.
- **Phase 2 (Foundational)**: needs Phase 1; blocks every story, because every probe calls these host operations.
- **US1 (Phase 3)** and **US2 (Phase 4)**: independent of each other once Phase 2 is done; both are P1, US1 first because the hostmem path is what 042-044 hang on.
- **US4 (Phase 5)**: needs US1 and US2 to have results to report.
- **US3 (Phase 6)**: independent of the memory stories; P2, so it follows them.
- **Phase 7**: everything.

### Parallel opportunities

- T005 and T006 (mock and ABI harness) are different files.
- T009, T014, T020 and T025 are four test files, each ahead of its own class.
- US1's runner work (T011, T012) is parallel to US2's probe (T014, T015) once Phase 2 lands.

---

## Implementation Strategy

**MVP**: Phases 1-3. That answers the question 041 and 042 most need - whether
host GPU memory reaches the guest at all - and records it for both GPUs in this
machine.

**Increment 2**: Phase 4 and 5, which turn both memory answers into capset bits
with tested fallbacks.

**Increment 3**: Phase 6, the interrupt stress, which needs a long guest run and
is the only part that cannot be judged on Linux alone.

---

## Notes

- Every class here owns state and is named in `spec.md`'s table; no static
  methods, no free functions outside the existing ABI boundary.
- A probe is an object that answers once: `ProbeResult` enforces it, because a
  capability bit whose evidence changed underneath it is worse than no bit.
- No probe may fail realize, and none may leave a handle behind. Both are
  asserted by `MockHost`, not by review.
