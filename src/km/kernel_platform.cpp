#include "km/kernel_platform.hpp"

#include <intrin.h>

/* dispmprt.h, unlike ntddk.h, declares its imports without a C linkage guard. */
extern "C" {
#include <dispmprt.h>
}

#include "core/miniport/ddi_table.hpp"
#include "core/platform/mutex.hpp"
#include "core/platform/wait_mutex.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

static_assert(sizeof(FAST_MUTEX) <= Mutex::storage_bytes, "Mutex storage too small for FAST_MUTEX");
static_assert(sizeof(KMUTEX) <= WaitMutex::storage_bytes, "WaitMutex storage too small for KMUTEX");
static_assert(alignof(FAST_MUTEX) <= 8u, "Mutex storage alignment too weak for FAST_MUTEX");
static_assert(sizeof(KSPIN_LOCK) == sizeof(uintptr_t), "SpinLock word is not a KSPIN_LOCK");
static_assert(static_cast<unsigned>(Pool::non_paged) == 0u && static_cast<unsigned>(Pool::paged) == 1u,
              "pool_flags_ is indexed by Pool");

/* One millisecond, in the 100 ns units ExSetTimerResolution counts. */
constexpr ULONG fine_clock_ticks = 10000u;

KernelPlatform::KernelPlatform(ULONG pool_tag) : pool_tag_(pool_tag), log_lock_()
{
    AV_SCOPE("KernelPlatform::KernelPlatform");
    KeInitializeSpinLock(&log_lock_);
}

void *KernelPlatform::allocate(size_t bytes, Pool pool)
{
    AV_SCOPE("KernelPlatform::allocate");
    return ExAllocatePool2(pool_flags_[static_cast<unsigned>(pool)], bytes, pool_tag_);
}

void KernelPlatform::release(void *block)
{
    AV_SCOPE("KernelPlatform::release");
    ExFreePoolWithTag(block, pool_tag_);
}

/* Any physical address, cached: virtio without ACCESS_PLATFORM addresses guest
 * physical memory directly. */
void *KernelPlatform::allocate_contiguous(size_t bytes)
{
    AV_SCOPE("KernelPlatform::allocate_contiguous");
    PHYSICAL_ADDRESS lowest = {};
    PHYSICAL_ADDRESS highest = {};
    PHYSICAL_ADDRESS boundary = {};
    highest.QuadPart = MAXLONGLONG;
    return MmAllocateContiguousMemorySpecifyCache(bytes, lowest, highest, boundary, MmCached);
}

uint64_t KernelPlatform::physical_address(void *block)
{
    AV_SCOPE("KernelPlatform::physical_address");
    return static_cast<uint64_t>(MmGetPhysicalAddress(block).QuadPart);
}

/* MmGetMdlPfnArray is the only way to the pages of a locked MDL; the span the
 * MDL describes bounds the read, because dxgkrnl's page count and ours coming
 * apart would have the device read memory the allocation does not own. */
Status KernelPlatform::read_page_frames(void *mdl, uint32_t first, uint32_t count, uint64_t *frames)
{
    AV_SCOPE("KernelPlatform::read_page_frames");
    auto *const descriptor = static_cast<MDL *>(mdl);
    if (descriptor == nullptr || frames == nullptr || count == 0u) return Status::invalid_parameter;
    const SIZE_T pages = ADDRESS_AND_SIZE_TO_SPAN_PAGES(MmGetMdlVirtualAddress(descriptor),
                                                       MmGetMdlByteCount(descriptor));
    if (first > pages || count > pages - first) return Status::invalid_parameter;
    const PFN_NUMBER *const page_frames = MmGetMdlPfnArray(descriptor);
    for (uint32_t page = 0u; page < count; page++)
        frames[page] = static_cast<uint64_t>(page_frames[first + page]) << PAGE_SHIFT;
    return Status::ok;
}

/* RAM is mapped cached, as the kernel's own mapping of it is: a view with any
 * other cache type is a conflict Driver Verifier reports. */
uint8_t *KernelPlatform::map_frame(uint64_t frame)
{
    AV_SCOPE("KernelPlatform::map_frame");
    PHYSICAL_ADDRESS address = {};
    address.QuadPart = static_cast<LONGLONG>(frame);
    return static_cast<uint8_t *>(MmMapIoSpaceEx(address, PAGE_SIZE, PAGE_READWRITE));
}

void KernelPlatform::unmap_frame(uint8_t *view)
{
    AV_SCOPE("KernelPlatform::unmap_frame");
    MmUnmapIoSpace(view, PAGE_SIZE);
}

void KernelPlatform::release_contiguous(void *block, size_t bytes)
{
    AV_SCOPE("KernelPlatform::release_contiguous");
    MmFreeContiguousMemorySpecifyCache(block, bytes, MmCached);
}

void KernelPlatform::zero(void *dst, size_t bytes)
{
    AV_SCOPE("KernelPlatform::zero");
    RtlZeroMemory(dst, bytes);
}

void KernelPlatform::copy(void *dst, const void *src, size_t bytes)
{
    AV_SCOPE("KernelPlatform::copy");
    RtlCopyMemory(dst, src, bytes);
}

Irql KernelPlatform::current_irql() const
{
    AV_SCOPE("KernelPlatform::current_irql");
    return static_cast<Irql>(KeGetCurrentIrql());
}

/* Driver Verifier's IRQL checking enforces ceilings at the real kernel calls;
 * the declared ceiling is checked by MockPlatform in every unit test. */
void KernelPlatform::require_irql_at_most(Irql, const char *)
{
    AV_SCOPE("KernelPlatform::require_irql_at_most");
}

Irql KernelPlatform::acquire_spin_lock(uintptr_t &word, LockRank)
{
    AV_SCOPE("KernelPlatform::acquire_spin_lock");
    return static_cast<Irql>(KeAcquireSpinLockRaiseToDpc(&word));
}

void KernelPlatform::release_spin_lock(uintptr_t &word, LockRank, Irql previous)
{
    AV_SCOPE("KernelPlatform::release_spin_lock");
    KeReleaseSpinLock(&word, static_cast<KIRQL>(previous));
}

/* TimerCallback is EXT_CALLBACK with void pointers, so it is passed as is. */
/* A high-resolution EX timer still expires on the system clock's tick, and a
 * guest's default tick is 15.625 ms: a 16.666 ms period then fires every
 * 31.25 ms, which is the 31.2 Hz vertical blank the in-guest suite measured.
 * Asking for a one-millisecond clock is what makes the period reachable; the
 * request is given back when the timer goes. */
void *KernelPlatform::create_timer(TimerCallback callback, void *context)
{
    AV_SCOPE("KernelPlatform::create_timer");
    PEX_TIMER timer = ExAllocateTimer(reinterpret_cast<PEXT_CALLBACK>(callback), context,
                                      EX_TIMER_HIGH_RESOLUTION);
    if (timer != nullptr) static_cast<void>(ExSetTimerResolution(fine_clock_ticks, TRUE));
    return timer;
}

/* A negative due time is relative; both it and the period are 100 ns units, as
 * a 17 that fired at the clock's millisecond proved (feature 038, US3). */
void KernelPlatform::start_timer(void *timer, uint64_t period)
{
    AV_SCOPE("KernelPlatform::start_timer");
    static_cast<void>(ExSetTimer(static_cast<PEX_TIMER>(timer), -static_cast<LONGLONG>(period),
                                 static_cast<LONGLONG>(period), nullptr));
}

void KernelPlatform::stop_timer(void *timer)
{
    AV_SCOPE("KernelPlatform::stop_timer");
    static_cast<void>(ExCancelTimer(static_cast<PEX_TIMER>(timer), nullptr));
}

void KernelPlatform::destroy_timer(void *timer)
{
    AV_SCOPE("KernelPlatform::destroy_timer");
    static_cast<void>(ExDeleteTimer(static_cast<PEX_TIMER>(timer), TRUE, TRUE, nullptr));
    static_cast<void>(ExSetTimerResolution(0u, FALSE));
}

void KernelPlatform::initialize_mutex(void *storage)
{
    AV_SCOPE("KernelPlatform::initialize_mutex");
    ExInitializeFastMutex(static_cast<PFAST_MUTEX>(storage));
}

void KernelPlatform::acquire_mutex(void *storage, LockRank)
{
    AV_SCOPE("KernelPlatform::acquire_mutex");
    ExAcquireFastMutex(static_cast<PFAST_MUTEX>(storage));
}

void KernelPlatform::release_mutex(void *storage, LockRank)
{
    AV_SCOPE("KernelPlatform::release_mutex");
    ExReleaseFastMutex(static_cast<PFAST_MUTEX>(storage));
}

void KernelPlatform::initialize_wait_mutex(void *storage)
{
    AV_SCOPE("KernelPlatform::initialize_wait_mutex");
    KeInitializeMutex(static_cast<PKMUTEX>(storage), 0u);
}

void KernelPlatform::acquire_wait_mutex(void *storage, LockRank)
{
    AV_SCOPE("KernelPlatform::acquire_wait_mutex");
    static_cast<void>(KeWaitForSingleObject(storage, Executive, KernelMode, FALSE, nullptr));
}

void KernelPlatform::release_wait_mutex(void *storage, LockRank)
{
    AV_SCOPE("KernelPlatform::release_wait_mutex");
    static_cast<void>(KeReleaseMutex(static_cast<PKMUTEX>(storage), FALSE));
}

uint64_t KernelPlatform::interrupt_time() const
{
    AV_SCOPE("KernelPlatform::interrupt_time");
    return KeQueryInterruptTime();
}

/* LONG64 and uint64_t may alias: they differ only in signedness. */
void KernelPlatform::store_atomic(uint64_t &target, uint64_t value)
{
    AV_SCOPE("KernelPlatform::store_atomic");
    static_cast<void>(InterlockedExchange64(reinterpret_cast<LONG64 *>(&target), static_cast<LONG64>(value)));
}

/* Adding zero answers the value as one atomic read. */
uint64_t KernelPlatform::load_atomic(uint64_t &source)
{
    AV_SCOPE("KernelPlatform::load_atomic");
    return static_cast<uint64_t>(InterlockedExchangeAdd64(reinterpret_cast<LONG64 *>(&source), 0));
}

uint64_t KernelPlatform::performance_counter(uint64_t &frequency) const
{
    AV_SCOPE("KernelPlatform::performance_counter");
    LARGE_INTEGER ticks_per_second = {};
    const LARGE_INTEGER count = KeQueryPerformanceCounter(&ticks_per_second);
    frequency = static_cast<uint64_t>(ticks_per_second.QuadPart);
    return static_cast<uint64_t>(count.QuadPart);
}

void KernelPlatform::stall(uint32_t microseconds)
{
    AV_SCOPE("KernelPlatform::stall");
    KeStallExecutionProcessor(microseconds);
}

uint8_t KernelPlatform::read_register_u8(void *address)
{
    AV_SCOPE("KernelPlatform::read_register_u8");
    return READ_REGISTER_UCHAR(static_cast<PUCHAR>(address));
}

uint16_t KernelPlatform::read_register_u16(void *address)
{
    AV_SCOPE("KernelPlatform::read_register_u16");
    return READ_REGISTER_USHORT(static_cast<PUSHORT>(address));
}

uint32_t KernelPlatform::read_register_u32(void *address)
{
    AV_SCOPE("KernelPlatform::read_register_u32");
    return READ_REGISTER_ULONG(static_cast<PULONG>(address));
}

void KernelPlatform::write_register_u8(void *address, uint8_t value)
{
    AV_SCOPE("KernelPlatform::write_register_u8");
    WRITE_REGISTER_UCHAR(static_cast<PUCHAR>(address), value);
}

void KernelPlatform::write_register_u16(void *address, uint16_t value)
{
    AV_SCOPE("KernelPlatform::write_register_u16");
    WRITE_REGISTER_USHORT(static_cast<PUSHORT>(address), value);
}

void KernelPlatform::write_register_u32(void *address, uint32_t value)
{
    AV_SCOPE("KernelPlatform::write_register_u32");
    WRITE_REGISTER_ULONG(static_cast<PULONG>(address), value);
}

void KernelPlatform::write_register_u64(void *address, uint64_t value)
{
    AV_SCOPE("KernelPlatform::write_register_u64");
    WRITE_REGISTER_ULONG64(static_cast<PULONG64>(address), value);
}

void KernelPlatform::log(const char *message)
{
    AV_SCOPE("KernelPlatform::log");
    KIRQL previous = 0;
    KeAcquireSpinLock(&log_lock_, &previous);
    DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_ERROR_LEVEL, "rvkgpu: %s\n", message);
    /* WRITE_PORT_BUFFER_UCHAR expands to __outbytestring, which clang-cl does
     * not provide; one byte at a time is the same port protocol. */
    for (const char *cursor = message; *cursor != '\0'; ++cursor)
        WRITE_PORT_UCHAR(reinterpret_cast<PUCHAR>(debug_port), static_cast<UCHAR>(*cursor));
    WRITE_PORT_UCHAR(reinterpret_cast<PUCHAR>(debug_port), '\n');
    KeReleaseSpinLock(&log_lock_, previous);
}

/* The table's layout is proven identical to DRIVER_INITIALIZATION_DATA by
 * ddi_table_layout.cpp. */
int32_t KernelPlatform::register_miniport(void *driver_object, void *registry_path,
                                          const DdiTable &table)
{
    AV_SCOPE("KernelPlatform::register_miniport");
    return DxgkInitialize(static_cast<PDRIVER_OBJECT>(driver_object),
                          static_cast<PUNICODE_STRING>(registry_path),
                          reinterpret_cast<PDRIVER_INITIALIZATION_DATA>(const_cast<DdiTable *>(&table)));
}

/* Broken invariants mean corrupted state; the kernel's own response to that is
 * a fail-fast, not continuing. */
void KernelPlatform::report_broken_invariant(const char *what, const char *function)
{
    AV_SCOPE("KernelPlatform::report_broken_invariant");
    DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_ERROR_LEVEL, "rvkgpu: invariant %s broken in %s\n",
               what, function);
    __fastfail(FAST_FAIL_INVALID_ARG);
}

}  // namespace av
