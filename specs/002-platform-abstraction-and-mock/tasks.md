---
description: "Task list for feature 002: platform abstraction and adversarial mock"
---

# Tasks: Platform abstraction and adversarial mock

**Input**: `specs/002-platform-abstraction-and-mock/` - plan.md, spec.md, research.md, data-model.md, contracts/platform.md

**Tests**: required and first (constitution IV). Each implementation task names the test task it makes pass.

## Phase 1: Setup - split the multi-class header (no behaviour change)

- [X] T001 Move `Status` to `src/core/status.hpp`, `ScopeExit` to `src/core/scope_exit.hpp`, `InvariantScope` to `src/core/invariant_scope.hpp`, `AV_UNTESTABLE` to `src/core/untestable.hpp`; `src/core/platform.hpp` keeps `Irql`, `Pool`, `Platform` and includes `status.hpp`; every file includes what it names; `ctest --test-dir build` passes unchanged

## Phase 2: Foundational - the mock that fails tests

- [X] T002 Write `tests/unit/test_mock_platform.cpp` covering, for each `Violation` kind in `tests/harness/violation.hpp` (irql_ceiling, allocation_at_dispatch, blocking_at_dispatch, lock_order, release_order, lock_stack_overflow, null_release, broken_invariant, time_overflow): provoke it through `MockPlatform`, `CHECK(platform.claim(kind))`, `CHECK(!platform.claim(kind))`; and in a nested scope destroy a mock with the violation unclaimed and `CHECK(TestRegistry::instance().take_current_failure())`; same for a live allocation and a held lock; plus time starts equal across reads, advances exactly, saturates on overflow; spin acquire raises to DISPATCH and restores; mutex acquire raises to APC and restores; zero-byte allocation returns null with no violation; fail-at-N beyond the count never fails. Must fail to compile now
- [X] T003 Add `tests/harness/violation.hpp` and `TestRegistry::take_current_failure()` in `tests/harness/test_registry.{hpp,cpp}`
- [X] T004 Extend `src/core/platform.hpp` with `LockRank` (`src/core/lock_rank.hpp`, empty scoped enum over `uint16_t`, comment stating strictly-increasing acquisition and that each rank is added with its lock), `acquire_spin_lock`, `release_spin_lock`, `initialize_mutex`, `acquire_mutex`, `release_mutex`, `interrupt_time` per `contracts/platform.md`, and make the destructor `protected` non-virtual (research R1)
- [X] T005 Rewrite `tests/harness/mock_platform.{hpp,cpp}` per `contracts/platform.md` and data-model.md: per-kind violation counts printed once at record time with detail, `claim`, `unclaimed_violations`, 16-entry lock stack of `{rank, previous}`, simulated IRQL transitions, `advance_time` with saturation, null release recorded, destructor failing the test for unclaimed violations, live allocations and held locks. T002 passes
- [X] T006 Replace `irql_was_violated()`/`invariant_was_broken()` checks in `tests/unit/test_gpu_virtual_address_space.cpp` with `claim(Violation::...)`, including claiming `allocation_at_dispatch` where a PASSIVE function is driven at DISPATCH; remove the old accessors; make `tests/fuzz/fuzz_address_space.cpp` trap when `platform.unclaimed_violations() != 0` after an input

**Checkpoint**: the mock is adversarial; commit. (Phases 2 and 3 share a commit: the new null-release rule failed every address-space test until T008.)

- [X] T008a Found during T005: `ScopeExit`/`InvariantScope` in their own headers need tests (`tests/unit/test_scope_exit.cpp`, `test_invariant_scope.cpp`); `tools/header_scan.py` read calls inside inline template bodies as declarations (now reads only the class body's brace level, test-first); `tools/check_coverage.py` counted each template instantiation's branches separately (now merged by source position, test-first). `grow()` also released the null table on the first growth; both releases go through `release_table()`.

## Phase 3: US1 - rule breaks in core fail tests (P1)

- [X] T007 [US1] Add `TEST(address_space_lifetime, destroying_before_any_reservation_releases_nothing)` and `..._after_a_failed_first_allocation_releases_nothing` to `tests/unit/test_gpu_virtual_address_space.cpp`; must fail with `null_release`
- [X] T008 [US1] Guard the table release in `GpuVirtualAddressSpace::~GpuVirtualAddressSpace` in `src/core/gpu_virtual_address_space.cpp`. T007 passes; coverage of the file stays 100%

**Checkpoint**: commit.

## Phase 4: US2 - core locks (P1)

- [X] T009 [P] [US2] Write `tests/unit/test_spin_lock.cpp` and `tests/unit/test_spin_lock_guard.cpp`: acquire/release raise and restore IRQL from PASSIVE and from DISPATCH; increasing ranks nest cleanly; equal and decreasing ranks record `lock_order`; out-of-order guard destruction impossible by scope but manual release out of order records `release_order`; acquire above DISPATCH records `irql_ceiling`; an early return inside a guard's scope releases. Must fail to compile
- [X] T010 [P] [US2] Write `tests/unit/test_mutex.cpp` and `tests/unit/test_mutex_guard.cpp`: construction initializes; guard raises to APC and restores; acquire at DISPATCH records `irql_ceiling` and `blocking_at_dispatch`; rank rules as for spin locks; early return releases. Must fail to compile
- [X] T011 [US2] Implement `src/core/spin_lock.{hpp,cpp}` and `src/core/spin_lock_guard.{hpp,cpp}` (ceilings DISPATCH). T009 passes
- [X] T012 [US2] Implement `src/core/mutex.{hpp,cpp}` (64-byte storage aligned 8, `storage_bytes` public constant, initialize in constructor at ceiling DISPATCH, acquire/release ceiling APC) and `src/core/mutex_guard.{hpp,cpp}`. T010 passes. Found: the mock's spin-lock acquire lowered an IRQL above DISPATCH to DISPATCH; it now only raises (test added)

**Checkpoint**: commit.

## Phase 5: US3 - exact time (P2)

Covered by T002/T005 (mock clock) - no separate core class consumes time yet.

## Phase 6: US4 - kernel translation (P2)

- [X] T013 [US4] Implement `src/km/kernel_platform.{hpp,cpp}`: `KernelPlatform final`, constructor takes the pool tag, overrides per research R3, `static constexpr POOL_FLAGS pool_flags_[]`, `static_assert` on `sizeof(FAST_MUTEX) <= av::Mutex::storage_bytes` and alignment; no conditional branch. `cmake --build build` links; `driver_image` passes
- [X] T014 [US4] Add a kernel-probe test to `tests/tools/test_kernel_build.py` that builds a probe including `kernel_platform.hpp` and instantiating `KernelPlatform` with a spin lock and mutex round trip, and asserts the image imports only `ntoskrnl.exe`

**Checkpoint**: commit.

## Phase 7: Polish

- [X] T015 [P] `CLAUDE.md` adaptations: polymorphic bases never deleted through a base pointer get a protected non-virtual destructor (no `operator delete` in the kernel); `docs/testing.md`: `lock_rank.hpp`, record-and-claim instead of abort, time and lock checks as implemented
- [X] T016 Run `tools/gate.sh`; fix anything; confirm no unclaimed violations printed anywhere in the unit suite output; merge to `master` per `.claude/commands/dev.md`
