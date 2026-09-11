# Research: Platform abstraction and adversarial mock

Verified on this machine, 2026-09-11, with the feature 001 kernel toolchain.

## R1. Polymorphic base destructor

**Decision**: `Platform` declares `protected: ~Platform() = default;`, non-virtual.

**Rationale**: A kernel probe with `virtual ~Base() = default` failed to link:
`undefined symbol: void __cdecl operator delete(void *, unsigned __int64)` -
the MSVC ABI emits a scalar deleting destructor in the vtable. With a protected
non-virtual destructor the same probe links. A platform is always a named
object (stack in tests, adapter member in the driver) and is never deleted
through `Platform *`; protected access makes that a compile error.

**Alternatives**: defining `operator delete` in `src/km` to bugcheck (a global
heap operator in a codebase with no heap operators); `-fno-...` flags to drop
deleting destructors (none exists for the MSVC ABI).

## R2. Where rules are enforced

**Decision**: The mock enforces every rule. `KernelPlatform` translates only;
its `require_irql_at_most` is empty.

**Rationale**: Constitution II allows no decision in `src/km`. Driver Verifier's
IRQL checking enforces the kernel side at the real API calls. Putting checks in
a non-virtual `Platform` base would give `Platform` a responsibility beyond
"the contract" and make it a stateless class with logic, which
`tools/check_design.py` rejects.

## R3. Kernel primitive mapping (all link against `ntoskrnl.lib`, probe-verified)

| Contract | Kernel |
|---|---|
| allocate | `ExAllocatePool2(pool_flags_[pool], bytes, tag)` - table, not branch |
| release | `ExFreePoolWithTag(block, tag)` (bugchecks on NULL - hence the mock's null-release rule) |
| zero / copy | `RtlZeroMemory` / `RtlCopyMemory` |
| current_irql | `KeGetCurrentIrql()`; PASSIVE 0, APC 1, DISPATCH 2 map 1:1; any higher value compares above `Irql::dispatch` |
| acquire / release spin lock | `KeAcquireSpinLockRaiseToDpc` / `KeReleaseSpinLock`; `KSPIN_LOCK` is `ULONG_PTR`, i.e. `uintptr_t` |
| initialize / acquire / release mutex | `ExInitializeFastMutex` / `ExAcquireFastMutex` / `ExReleaseFastMutex`; raises to APC, not reentrant |
| interrupt_time | `KeQueryInterruptTime()` - 100 ns since boot, monotonic |
| log | `DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_ERROR_LEVEL, "rvkgpu: %s\n", ...)` |
| report_broken_invariant | log, then `__fastfail(FAST_FAIL_INVALID_ARG)` (bugcheck 0x139 with the code) |

`sizeof(FAST_MUTEX) == 56`, `alignof == 8` on x64: core reserves 64 bytes
aligned to 8; `src/km` `static_assert`s both.

`__fastfail` rather than `KeBugCheckEx` with an invented code: bugcheck codes
are Microsoft-assigned, and a fail-fast is the kernel's own response to
detected state corruption. It is not a branch.

## R4. Violation bookkeeping in the mock

**Decision**: a count per violation kind; recording prints one line with the
detail; `claim(kind)` decrements and returns whether it was non-zero; the
destructor reports every non-zero count, live allocation and held lock through
`TestRegistry`. `unclaimed_violations()` sums the counts (fuzz targets trap on
non-zero).

**Rationale**: fixed-size, no allocation; the detail is already printed at the
point of decision (Principle IX), so storing it again buys nothing.

## R5. Lock stack

**Decision**: 16 entries of `{rank, previous IRQL}`. Acquire requires the rank
strictly above the top; release must match the top. Overflow records a
violation and stops tracking depth.

## R6. Why double reports are right

Acquiring a mutex at DISPATCH breaks two rules - `Mutex::acquire`'s declared
ceiling (APC) and "never block at DISPATCH" - and records both; allocating at
DISPATCH inside a PASSIVE-ceiling function likewise. A test claims each. The
kinds stay distinct because a DISPATCH-ceiling function that allocates breaks
only the second rule.

## R7. Existing defect

`GpuVirtualAddressSpace::~GpuVirtualAddressSpace` calls
`platform_.release(ranges_)` unconditionally; `ranges_` is null before the first
`grow`. Under the kernel this is `ExFreePoolWithTag(NULL, ...)`, a bugcheck. The
null-release rule turns it into a failing test (`AddressSpaceFixture` with no
reservation is enough), then a guarded release fixes it.
