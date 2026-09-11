# Contract: `av::Platform`

What core may call, what each implementation guarantees. `KernelPlatform`
column: the one kernel call. `MockPlatform` column: what it checks.

| Member | Kernel | Mock records |
|---|---|---|
| `void *allocate(size_t bytes, Pool pool)` | `ExAllocatePool2` | `allocation_at_dispatch` if IRQL >= DISPATCH (returns null); honours `fail_allocation_at`; null for 0 bytes |
| `void release(void *block)` | `ExFreePoolWithTag` | `null_release` for null; aborts for an untracked pointer |
| `void zero(void *, size_t)` / `void copy(void *, const void *, size_t)` | `RtlZeroMemory` / `RtlCopyMemory` | - |
| `Irql current_irql() const` | `KeGetCurrentIrql` | simulated level |
| `void require_irql_at_most(Irql, const char *function)` | nothing (Driver Verifier) | `irql_ceiling` if above |
| `Irql acquire_spin_lock(uintptr_t &word, LockRank)` | `KeAcquireSpinLockRaiseToDpc` | `lock_order`, `lock_stack_overflow`; raises to DISPATCH |
| `void release_spin_lock(uintptr_t &word, LockRank, Irql previous)` | `KeReleaseSpinLock` | `release_order`; restores `previous` |
| `void initialize_mutex(void *storage)` | `ExInitializeFastMutex` | - |
| `void acquire_mutex(void *storage, LockRank)` | `ExAcquireFastMutex` | `blocking_at_dispatch`, `lock_order`, `lock_stack_overflow`; raises to APC |
| `void release_mutex(void *storage, LockRank)` | `ExReleaseFastMutex` | `release_order`; restores saved IRQL |
| `uint64_t interrupt_time() const` | `KeQueryInterruptTime` | test-controlled clock |
| `void store_atomic(uint64_t &, uint64_t)` / `uint64_t load_atomic(uint64_t &)` | `InterlockedExchange64` / `InterlockedExchangeAdd64(&v, 0)` | plain store and load (one thread) |
| `uint64_t performance_counter(uint64_t &frequency) const` | `KeQueryPerformanceCounter` | the test clock at 10 000 000 per second |
| `void *create_timer(TimerCallback, void *context)` | `ExAllocateTimer(EX_TIMER_HIGH_RESOLUTION)` | `allocation_at_dispatch` (returns null); honours `fail_allocation_at`; null past `max_timers` |
| `void start_timer(void *timer, uint64_t period)` | `ExSetTimer`, relative due time, periodic | `irql_ceiling` above DISPATCH; `timer_misuse` for an unknown timer or a zero period |
| `void stop_timer(void *timer)` | `ExCancelTimer` | `irql_ceiling` above DISPATCH; `timer_misuse` |
| `void destroy_timer(void *timer)` | `ExDeleteTimer(cancel, wait)` | `blocking_at_dispatch`; `null_release`; `timer_misuse` |
| `void log(const char *)` | `DbgPrintEx` | printed when `AV_TEST_VERBOSE` is set |
| `void report_broken_invariant(const char *what, const char *function)` | log + `__fastfail` | `broken_invariant` |

## Mock-only test surface

`fail_allocation_at(n)`, `set_irql(irql)`, `advance_time(ticks)`,
`run_timers(ticks)` (the only way a timer fires: at DISPATCH, earliest due first),
`bool claim(Violation)`, `uint32_t unclaimed_violations() const`,
`uint32_t live_allocations() const`, `uint32_t allocations_made() const`,
`uint32_t locks_held() const`. Destruction fails the running test for any
unclaimed violation, live allocation, held lock, or timer not destroyed.

`TestRegistry::take_current_failure()` returns whether the running test has
recorded a failure and clears it, so a test can prove that destroying a mock
with an unclaimed violation fails a test - without failing itself.
