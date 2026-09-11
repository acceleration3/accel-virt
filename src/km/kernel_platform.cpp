#include "kernel_platform.hpp"

#include "mutex.hpp"

namespace av {

static_assert(sizeof(FAST_MUTEX) <= Mutex::storage_bytes, "Mutex storage too small for FAST_MUTEX");
static_assert(alignof(FAST_MUTEX) <= 8u, "Mutex storage alignment too weak for FAST_MUTEX");
static_assert(sizeof(KSPIN_LOCK) == sizeof(uintptr_t), "SpinLock word is not a KSPIN_LOCK");
static_assert(static_cast<unsigned>(Pool::non_paged) == 0u && static_cast<unsigned>(Pool::paged) == 1u,
              "pool_flags_ is indexed by Pool");

KernelPlatform::KernelPlatform(ULONG pool_tag) : pool_tag_(pool_tag) {}

void *KernelPlatform::allocate(size_t bytes, Pool pool)
{
    return ExAllocatePool2(pool_flags_[static_cast<unsigned>(pool)], bytes, pool_tag_);
}

void KernelPlatform::release(void *block) { ExFreePoolWithTag(block, pool_tag_); }

/* Any physical address, cached: virtio without ACCESS_PLATFORM addresses guest
 * physical memory directly. */
void *KernelPlatform::allocate_contiguous(size_t bytes)
{
    PHYSICAL_ADDRESS lowest = {};
    PHYSICAL_ADDRESS highest = {};
    PHYSICAL_ADDRESS boundary = {};
    highest.QuadPart = MAXLONGLONG;
    return MmAllocateContiguousMemorySpecifyCache(bytes, lowest, highest, boundary, MmCached);
}

uint64_t KernelPlatform::physical_address(void *block)
{
    return static_cast<uint64_t>(MmGetPhysicalAddress(block).QuadPart);
}

void KernelPlatform::release_contiguous(void *block, size_t bytes)
{
    MmFreeContiguousMemorySpecifyCache(block, bytes, MmCached);
}

void KernelPlatform::zero(void *dst, size_t bytes) { RtlZeroMemory(dst, bytes); }

void KernelPlatform::copy(void *dst, const void *src, size_t bytes)
{
    RtlCopyMemory(dst, src, bytes);
}

Irql KernelPlatform::current_irql() const { return static_cast<Irql>(KeGetCurrentIrql()); }

/* Driver Verifier's IRQL checking enforces ceilings at the real kernel calls;
 * the declared ceiling is checked by MockPlatform in every unit test. */
void KernelPlatform::require_irql_at_most(Irql, const char *) {}

Irql KernelPlatform::acquire_spin_lock(uintptr_t &word, LockRank)
{
    return static_cast<Irql>(KeAcquireSpinLockRaiseToDpc(&word));
}

void KernelPlatform::release_spin_lock(uintptr_t &word, LockRank, Irql previous)
{
    KeReleaseSpinLock(&word, static_cast<KIRQL>(previous));
}

void KernelPlatform::initialize_mutex(void *storage)
{
    ExInitializeFastMutex(static_cast<PFAST_MUTEX>(storage));
}

void KernelPlatform::acquire_mutex(void *storage, LockRank)
{
    ExAcquireFastMutex(static_cast<PFAST_MUTEX>(storage));
}

void KernelPlatform::release_mutex(void *storage, LockRank)
{
    ExReleaseFastMutex(static_cast<PFAST_MUTEX>(storage));
}

uint64_t KernelPlatform::interrupt_time() const { return KeQueryInterruptTime(); }

void KernelPlatform::stall(uint32_t microseconds) { KeStallExecutionProcessor(microseconds); }

void KernelPlatform::log(const char *message)
{
    DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_ERROR_LEVEL, "rvkgpu: %s\n", message);
}

/* Broken invariants mean corrupted state; the kernel's own response to that is
 * a fail-fast, not continuing. */
void KernelPlatform::report_broken_invariant(const char *what, const char *function)
{
    DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_ERROR_LEVEL, "rvkgpu: invariant %s broken in %s\n",
               what, function);
    __fastfail(FAST_FAIL_INVALID_ARG);
}

}  // namespace av
