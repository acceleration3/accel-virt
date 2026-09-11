# Data model: Platform abstraction and adversarial mock

## Core (`src/core`)

| Type | State | Rules |
|---|---|---|
| `Status`, `Irql`, `Pool` (enums) | - | `Irql` ordered passive < apc < dispatch < isr |
| `LockRank` (enum, `uint16_t`) | - | declared once in `lock_rank.hpp`; named ranks added with their lock |
| `Platform` (interface) | none | protected non-virtual destructor; pure virtuals only |
| `SpinLock` | `Platform &platform_`, `uintptr_t word_` (kernel lock), `LockRank rank_` | acquire/release ceiling DISPATCH |
| `SpinLockGuard` | `SpinLock &lock_`, `Irql previous_` | acquires in constructor, releases in destructor |
| `Mutex` | `Platform &platform_`, `unsigned char storage_[64]` (align 8), `LockRank rank_` | initialized in constructor (ceiling DISPATCH); acquire/release ceiling APC |
| `MutexGuard` | `Mutex &mutex_` | acquires in constructor, releases in destructor |
| `ScopeExit<F>`, `InvariantScope<T>` | unchanged | moved to own headers |

## Mock (`tests/harness`)

| Type | State |
|---|---|
| `Violation` (enum) | irql_ceiling, allocation_at_dispatch, blocking_at_dispatch, lock_order, release_order, lock_stack_overflow, null_release, broken_invariant, time_overflow |
| `MockPlatform` | live allocation table (4096), allocation sequence, fail-at, IRQL, held-lock stack (16 x {rank, previous IRQL}), depth, per-kind violation counts, interrupt time, verbose flag |

Transitions (mock IRQL): spin acquire -> DISPATCH, spin release -> saved;
mutex acquire -> APC (saved on the lock stack), mutex release -> saved.
