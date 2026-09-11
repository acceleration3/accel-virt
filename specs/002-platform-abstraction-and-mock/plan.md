# Implementation Plan: Platform abstraction and adversarial mock

**Branch**: `002-platform-abstraction-and-mock` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

Complete the platform contract (locks, mutex, time), give core lock objects
with scope guards, add the kernel implementation as pure translation, and make
the mock adversarial for real: every rule break is recorded, and an unclaimed
one fails the test. Split the multi-class `platform.hpp`. Fix the null-table
release the new rule exposes.

## Technical Context

**Language**: C++23 freestanding (`src/core`, `src/km`), hosted C++ in `tests/harness`.
**Dependencies**: none new; kernel APIs from the fetched kit (research R3).
**Testing**: L1 unit tests under the mock; kernel platform proven by the driver build and `driver_image`.
**Constraints**: no allocation, blocking or mutex at DISPATCH; no branch in `src/km`; no `operator delete`.
**Scale**: 4 core classes, 1 kernel class, ~9 violation kinds.

## Constitution Check

| Principle | Status |
|---|---|
| II portable core / translation shim | Pass - `KernelPlatform` has no branch; pool flags by table |
| III objects with state | Pass - see classes table; `Platform` is a pure interface (the one allowed stateless shape) |
| IV test-first, exhaustive | Pass - tests per class and per violation kind written first; mock's own behaviour tested |
| VII IRQL and locking typed | Pass, with the interpretation recorded in the spec (enforced in the native test build) |
| VIII no speculative structure | Pass - no events/timers/DPCs; ranks added with their locks |
| Freestanding adaptation | `Platform` destructor protected non-virtual (R1); `CLAUDE.md` updated in the same change |

Post-design re-check: unchanged.

## Classes this feature introduces

See the table in [spec.md](spec.md) (filled).

## Project Structure

```text
src/core/
├── platform.hpp          Platform interface, Irql, Pool (split)
├── status.hpp            Status
├── scope_exit.hpp        ScopeExit
├── invariant_scope.hpp   InvariantScope
├── untestable.hpp        AV_UNTESTABLE
├── lock_rank.hpp         LockRank
├── spin_lock.{hpp,cpp}   SpinLockGuard in spin_lock_guard.{hpp,cpp}
├── mutex.{hpp,cpp}       MutexGuard in mutex_guard.{hpp,cpp}
└── gpu_virtual_address_space.cpp   null-release fix
src/km/kernel_platform.{hpp,cpp}
tests/harness/
├── violation.hpp         Violation
├── mock_platform.{hpp,cpp}   extended
└── test_registry.{hpp,cpp}   take_current_failure()
tests/unit/harness/platform/test_mock_platform.cpp, test_spin_lock.cpp, test_spin_lock_guard.cpp,
          test_mutex.cpp, test_mutex_guard.cpp, test_gpu_virtual_address_space.cpp (claims)
tests/fuzz/fuzz_address_space.cpp   trap on unclaimed violations
CLAUDE.md, docs/testing.md          destructor adaptation; lock_rank.hpp; record-and-claim
```

## Complexity Tracking

None.
