/* Before the header's ntddk.h, which it includes and must follow: attaching to
 * another process to take a view out of it is declared only in ntifs.h. */
#include <ntifs.h>

#include "km/kernel_platform.hpp"

#include "proto/resources/guest_page_list.hpp"

#include <intrin.h>

/* dispmprt.h, unlike ntddk.h, declares its imports without a C linkage guard. */
extern "C" {
#include <dispmprt.h>
}

/* DEVPKEY_Device_DeviceDesc is defined, not only declared, in this one unit. */
#include <initguid.h>
#include <devpkey.h>

#include "miniport/driver/ddi_table.hpp"
#include "core/platform/mutex.hpp"
#include "core/platform/wait_mutex.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

static_assert(sizeof(FAST_MUTEX) <= Mutex::storage_bytes, "Mutex storage too small for FAST_MUTEX");
static_assert(sizeof(KMUTEX) <= WaitMutex::storage_bytes, "WaitMutex storage too small for KMUTEX");
static_assert(alignof(FAST_MUTEX) <= 8u, "Mutex storage alignment too weak for FAST_MUTEX");
static_assert(sizeof(KSPIN_LOCK) == sizeof(uintptr_t), "SpinLock word is not a KSPIN_LOCK");
static_assert(static_cast<unsigned>(Pool::non_paged) == 0u && static_cast<unsigned>(Pool::paged) == 1u,
              "pool_flags_ is indexed by Pool");

KernelPlatform::KernelPlatform(ULONG pool_tag) : pool_tag_(pool_tag), log_(), bugcheck_record_()
{
    AV_SCOPE("KernelPlatform::KernelPlatform");
    KeInitializeCallbackRecord(&bugcheck_record_);
    if (log_.start() != Status::ok) log("avgpu: the log thread did not start; lines go out on the caller's thread");
}

KernelPlatform::~KernelPlatform()
{
    AV_SCOPE("KernelPlatform::~KernelPlatform");
    log_.stop();
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

/* A committed pagefile section whose pages are locked through a view of it in
 * system space: they stay where they are until release_pages, as a device
 * holding their addresses needs, and a process maps the section itself (see
 * map_pages_to_process). Demand-zero, so zeroed. The record is the locked MDL,
 * which read_page_frames reads like any other, with the section and its
 * system view ahead of it in the same block. The MDL's byte count is a ULONG. */
void *KernelPlatform::allocate_pages(uint64_t bytes)
{
    AV_SCOPE("KernelPlatform::allocate_pages");
    if (bytes == 0u || bytes > max_mapping_bytes || (bytes & (PAGE_SIZE - 1u)) != 0u) return nullptr;
    OBJECT_ATTRIBUTES attributes;
    InitializeObjectAttributes(&attributes, nullptr, OBJ_KERNEL_HANDLE, nullptr, nullptr);
    LARGE_INTEGER size = {};
    size.QuadPart = static_cast<LONGLONG>(bytes);
    HANDLE handle = nullptr;
    if (!NT_SUCCESS(ZwCreateSection(&handle, SECTION_ALL_ACCESS, &attributes, &size, PAGE_READWRITE, SEC_COMMIT,
                                    nullptr)))
        return nullptr;
    PVOID          section = nullptr;
    const NTSTATUS referenced = ObReferenceObjectByHandle(handle, SECTION_ALL_ACCESS, nullptr, KernelMode, &section,
                                                          nullptr);
    ZwClose(handle);
    if (!NT_SUCCESS(referenced)) return nullptr;
    ScopeExit drop_section([section] { ObDereferenceObject(section); });
    PVOID  system_view = nullptr;
    SIZE_T view_bytes = 0u;
    if (!NT_SUCCESS(MmMapViewInSystemSpace(section, &system_view, &view_bytes))) return nullptr;
    ScopeExit   unmap_view([system_view] { MmUnmapViewInSystemSpace(system_view); });
    const SIZE_T mdl_bytes = MmSizeOfMdl(system_view, static_cast<SIZE_T>(bytes));
    auto *const  block = static_cast<SectionPages *>(
        ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(SectionPages) + mdl_bytes, pool_tag_));
    if (block == nullptr) return nullptr;
    ScopeExit   free_block([this, block] { ExFreePoolWithTag(block, pool_tag_); });
    auto *const mdl = reinterpret_cast<PMDL>(block + 1);
    MmInitializeMdl(mdl, system_view, static_cast<SIZE_T>(bytes));
    __try {
        MmProbeAndLockPages(mdl, KernelMode, IoWriteAccess);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    *block = SectionPages{section, system_view};
    drop_section.dismiss();
    unmap_view.dismiss();
    free_block.dismiss();
    return mdl;
}

/* The section itself lives on in any view a process still has of it. */
void KernelPlatform::release_pages(void *pages)
{
    AV_SCOPE("KernelPlatform::release_pages");
    auto *const          mdl = static_cast<PMDL>(pages);
    SectionPages *const block = reinterpret_cast<SectionPages *>(mdl) - 1;
    MmUnlockPages(mdl);
    MmUnmapViewInSystemSpace(block->system_view);
    ObDereferenceObject(block->section);
    ExFreePoolWithTag(block, pool_tag_);
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

/* The MDL's page array is written from the list, as map_to_process writes a
 * BAR's: the pages are an allocation's, which dxgkrnl keeps locked while the
 * aperture maps them. Write-combined, like every CPU mapping of this adapter's
 * allocations: the memory manager keeps one cache type per page, and a view
 * asked for as cached still read uncached (PresentBlit::copy_pixels). */
uint8_t *KernelPlatform::map_pages(const GuestPageList &pages, void *&mapping)
{
    AV_SCOPE("KernelPlatform::map_pages");
    mapping = nullptr;
    const uint32_t count = pages.count();
    if (count == 0u || uint64_t{count} * PAGE_SIZE > max_mapping_bytes) return nullptr;
    PMDL const mdl = IoAllocateMdl(nullptr, count * static_cast<ULONG>(PAGE_SIZE), FALSE, FALSE, nullptr);
    if (mdl == nullptr) return nullptr;
    PFN_NUMBER *const frames = MmGetMdlPfnArray(mdl);
    for (uint32_t page = 0u; page < count; page++) frames[page] = static_cast<PFN_NUMBER>(pages.frame(page) >> PAGE_SHIFT);
    mdl->MdlFlags |= MDL_PAGES_LOCKED;
    PVOID const view = MmMapLockedPagesSpecifyCache(mdl, KernelMode, MmWriteCombined, nullptr, FALSE,
                                                    NormalPagePriority | MdlMappingNoExecute);
    if (view == nullptr) {
        IoFreeMdl(mdl);
        return nullptr;
    }
    mapping = mdl;
    return static_cast<uint8_t *>(view);
}

void KernelPlatform::unmap_pages(uint8_t *view, void *mapping)
{
    AV_SCOPE("KernelPlatform::unmap_pages");
    MmUnmapLockedPages(view, static_cast<PMDL>(mapping));
    IoFreeMdl(static_cast<PMDL>(mapping));
}

/* The BAR's pages have no PFN database entries for MmBuildMdlForNonPagedPool
 * to read, so the MDL's page array is written by hand; they never move, which
 * is what MDL_PAGES_LOCKED claims. Write-back, because the host maps the same
 * pages write-back and an alias of any other type reads stale data
 * (docs/host-window.md). */
Status KernelPlatform::map_to_process(uint64_t physical, uint64_t bytes, PageCaching caching, uint64_t &view,
                                      void *&mapping)
{
    AV_SCOPE("KernelPlatform::map_to_process");
    view = 0u;
    mapping = nullptr;
    if (bytes == 0u || bytes > max_mapping_bytes) return Status::no_memory;
    PMDL const mdl = IoAllocateMdl(reinterpret_cast<PVOID>(physical), static_cast<ULONG>(bytes), FALSE, FALSE, nullptr);
    if (mdl == nullptr) return Status::no_memory;
    PFN_NUMBER *const frames = MmGetMdlPfnArray(mdl);
    const PFN_NUMBER  first = static_cast<PFN_NUMBER>(physical >> PAGE_SHIFT);
    for (uint64_t page = 0u; page < bytes >> PAGE_SHIFT; page++) frames[page] = first + page;
    mdl->MdlFlags |= MDL_PAGES_LOCKED;
    return map_mdl_to_process(mdl, bytes, caching == PageCaching::write_combined ? MmWriteCombined : MmCached, view,
                              mapping);
}

/* A view of the pages' section: the process's own memory, which stays valid
 * for as long as the process has the view, whatever the driver has let go of
 * since (see unmap_from_process). A 32-bit process addresses only its low
 * 4 GiB; a zero-bits value past 32 is the mask the view must fit under. */
Status KernelPlatform::map_pages_to_process(void *pages, uint64_t &view, void *&mapping)
{
    AV_SCOPE("KernelPlatform::map_pages_to_process");
    return map_section_to_process(pages, SECTION_MAP_READ | SECTION_MAP_WRITE, PAGE_READWRITE, view, mapping);
}

/* The handle can map only for reading, so the process cannot make the view
 * writable: measured in a Windows 11 guest, VirtualProtect to PAGE_READWRITE
 * on such a view fails with ERROR_INVALID_PARAMETER. */
Status KernelPlatform::map_pages_to_process_read_only(void *pages, uint64_t &view, void *&mapping)
{
    AV_SCOPE("KernelPlatform::map_pages_to_process_read_only");
    return map_section_to_process(pages, SECTION_MAP_READ, PAGE_READONLY, view, mapping);
}

Status KernelPlatform::map_section_to_process(void *pages, ACCESS_MASK access, ULONG protection, uint64_t &view,
                                              void *&mapping)
{
    AV_SCOPE("KernelPlatform::map_section_to_process");
    view = 0u;
    mapping = nullptr;
    auto *const               mdl = static_cast<PMDL>(pages);
    const SectionPages *const block = reinterpret_cast<SectionPages *>(mdl) - 1;
    auto *const               record =
        static_cast<ProcessMapping *>(ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(ProcessMapping), pool_tag_));
    if (record == nullptr) return Status::no_memory;
    ScopeExit free_record([this, record] { ExFreePoolWithTag(record, pool_tag_); });
    HANDLE    handle = nullptr;
    if (!NT_SUCCESS(ObOpenObjectByPointer(block->section, OBJ_KERNEL_HANDLE, nullptr, access, nullptr, KernelMode,
                                          &handle)))
        return Status::no_memory;
    const bool      narrow = IoIs32bitProcess(nullptr) != FALSE;
    const ULONG_PTR zero_bits = narrow ? static_cast<ULONG_PTR>(wow64_address_limit - 1u) : 0u;
    PVOID           user = nullptr;
    SIZE_T          user_bytes = 0u;
    const NTSTATUS  mapped = ZwMapViewOfSection(handle, ZwCurrentProcess(), &user, zero_bits, 0u, nullptr,
                                                &user_bytes, ViewUnmap, 0u, protection);
    ZwClose(handle);
    if (!NT_SUCCESS(mapped)) return Status::no_memory;
    if (narrow && reinterpret_cast<uint64_t>(user) + user_bytes > wow64_address_limit) {
        static_cast<void>(ZwUnmapViewOfSection(ZwCurrentProcess(), user));
        return Status::no_memory;
    }
    PEPROCESS const process = PsGetCurrentProcess();
    ObReferenceObject(process);
    *record = ProcessMapping{nullptr, user, process};
    free_record.dismiss();
    view = reinterpret_cast<uint64_t>(user);
    mapping = record;
    return Status::ok;
}

/* A user-mode mapping the process's address space refuses is raised as an
 * exception, not answered with null. */
Status KernelPlatform::map_mdl_to_process(PMDL mdl, uint64_t bytes, MEMORY_CACHING_TYPE caching, uint64_t &view,
                                          void *&mapping)
{
    AV_SCOPE("KernelPlatform::map_mdl_to_process");
    ScopeExit free_mdl([mdl] { IoFreeMdl(mdl); });
    auto *const record =
        static_cast<ProcessMapping *>(ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(ProcessMapping), pool_tag_));
    if (record == nullptr) return Status::no_memory;
    ScopeExit free_record([this, record] { ExFreePoolWithTag(record, pool_tag_); });
    PVOID user = nullptr;
    __try {
        user = MmMapLockedPagesSpecifyCache(mdl, UserMode, caching, nullptr, FALSE, NormalPagePriority);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        user = nullptr;
    }
    if (user == nullptr) return Status::no_memory;
    /* A 32-bit process addresses only its low 4 GiB. The memory manager places
     * a WoW64 process's views there; one that ended past it would be truncated
     * by the process into some other address, so it is taken back. */
    if (IoIs32bitProcess(nullptr) && reinterpret_cast<uint64_t>(user) + bytes > wow64_address_limit) {
        MmUnmapLockedPages(user, mdl);
        return Status::no_memory;
    }
    PEPROCESS const process = PsGetCurrentProcess();
    ObReferenceObject(process);
    *record = ProcessMapping{mdl, user, process};
    free_mdl.dismiss();
    free_record.dismiss();
    view = reinterpret_cast<uint64_t>(user);
    mapping = record;
    return Status::ok;
}

/* A view comes down in the process it was made in, and not at all if that has
 * exited, which took the view with its address space. A device memory view is
 * taken out of a process still running by attaching to it. A view of a pages
 * section is not: a teardown from another process is dxgkrnl stopping the
 * adapter under this one, whose threads may be in the ring this moment - a
 * live driver upgrade faulted DWM and every Direct3D process in
 * TransportRing::read_head. That view stays, memory the section keeps valid,
 * until the process unmaps it or exits. dxgkrnl tears down attached to the
 * process, so it is the calling thread's own process that is asked. */
void KernelPlatform::unmap_from_process(void *mapping)
{
    AV_SCOPE("KernelPlatform::unmap_from_process");
    auto *const record = static_cast<ProcessMapping *>(mapping);
    const bool  here = PsGetCurrentProcess() == record->process;
    if (record->mdl == nullptr) {
        /* A process that unmapped the view itself leaves nothing to take. */
        if (PsGetThreadProcess(PsGetCurrentThread()) == record->process)
            static_cast<void>(ZwUnmapViewOfSection(ZwCurrentProcess(), record->view));
    } else if (here) {
        MmUnmapLockedPages(record->view, record->mdl);
    } else if (PsGetProcessExitStatus(record->process) == STATUS_PENDING) {
        KAPC_STATE attached = {};
        KeStackAttachProcess(record->process, &attached);
        MmUnmapLockedPages(record->view, record->mdl);
        KeUnstackDetachProcess(&attached);
    }
    if (record->mdl != nullptr) IoFreeMdl(record->mdl);
    ObDereferenceObject(record->process);
    ExFreePoolWithTag(record, pool_tag_);
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

/* KeTryToAcquireSpinLockAtDpcLevel wants DISPATCH already; a refusal gives the
 * raise back so the caller sees no change. */
bool KernelPlatform::try_acquire_spin_lock(uintptr_t &word, LockRank, Irql &previous)
{
    AV_SCOPE("KernelPlatform::try_acquire_spin_lock");
    KIRQL raised_from = 0;
    KeRaiseIrql(DISPATCH_LEVEL, &raised_from);
    if (KeTryToAcquireSpinLockAtDpcLevel(&word) != FALSE) {
        previous = static_cast<Irql>(raised_from);
        return true;
    }
    KeLowerIrql(raised_from);
    return false;
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

bool KernelPlatform::try_acquire_mutex(void *storage, LockRank)
{
    AV_SCOPE("KernelPlatform::try_acquire_mutex");
    return ExTryToAcquireFastMutex(static_cast<PFAST_MUTEX>(storage)) != FALSE;
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

/* Precise, not KeQueryInterruptTime: that one moves only on the clock tick -
 * 15.6 ms unless some process asked the system for finer - and the display's
 * phase and scanline are read off this clock (VblankPhase). */
uint64_t KernelPlatform::interrupt_time() const
{
    AV_SCOPE("KernelPlatform::interrupt_time");
    ULONG64 counter = 0u;
    return KeQueryInterruptTimePrecise(&counter);
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
    DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_ERROR_LEVEL, "avgpu: %s\n", message);
    log_.write(message);
}

void KernelPlatform::log_at_bugcheck(const char *message)
{
    AV_SCOPE("KernelPlatform::log_at_bugcheck");
    log_.write_now(message);
}

/* BugcheckRoutine is KBUGCHECK_CALLBACK_ROUTINE with ULONG spelled uint32_t,
 * which on this ABI is the same type in the same register. */
bool KernelPlatform::register_bugcheck(void *buffer, size_t bytes, BugcheckRoutine routine)
{
    AV_SCOPE("KernelPlatform::register_bugcheck");
    static constexpr char component[] = "avgpu-trace";
    return KeRegisterBugCheckCallback(&bugcheck_record_, reinterpret_cast<PKBUGCHECK_CALLBACK_ROUTINE>(routine),
                                      buffer, static_cast<ULONG>(bytes),
                                      const_cast<PUCHAR>(reinterpret_cast<const UCHAR *>(component))) != FALSE;
}

void KernelPlatform::deregister_bugcheck()
{
    AV_SCOPE("KernelPlatform::deregister_bugcheck");
    static_cast<void>(KeDeregisterBugCheckCallback(&bugcheck_record_));
}

/* UserMode: the handle came from a process, so its access is checked against
 * what that process was granted - an event it may only wait on is refused. */
Status KernelPlatform::reference_event(uint64_t handle, void *&event)
{
    AV_SCOPE("KernelPlatform::reference_event");
    event = nullptr;
    PVOID          object = nullptr;
    const NTSTATUS status = ObReferenceObjectByHandle(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(handle)),
                                                      EVENT_MODIFY_STATE, *ExEventObjectType, UserMode, &object,
                                                      nullptr);
    if (!NT_SUCCESS(status)) return Status::invalid_parameter;
    event = object;
    return Status::ok;
}

void KernelPlatform::signal_event(void *event)
{
    AV_SCOPE("KernelPlatform::signal_event");
    static_cast<void>(KeSetEvent(static_cast<PKEVENT>(event), IO_NO_INCREMENT, FALSE));
}

void KernelPlatform::release_event(void *event)
{
    AV_SCOPE("KernelPlatform::release_event");
    ObDereferenceObject(event);
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

Status KernelPlatform::set_device_description(void *physical_device_object, const uint16_t *text, uint32_t bytes)
{
    AV_SCOPE("KernelPlatform::set_device_description");
    const NTSTATUS status = IoSetDevicePropertyData(static_cast<PDEVICE_OBJECT>(physical_device_object),
                                                    &DEVPKEY_Device_DeviceDesc, LOCALE_NEUTRAL,
                                                    PLUGPLAY_PROPERTY_PERSISTENT, DEVPROP_TYPE_STRING, bytes,
                                                    const_cast<uint16_t *>(text));
    return NT_SUCCESS(status) ? Status::ok : Status::not_supported;
}

Status KernelPlatform::write_driver_value(void *physical_device_object, const char *name, ULONG type,
                                          const void *bytes, ULONG size)
{
    AV_SCOPE("KernelPlatform::write_driver_value");
    WCHAR  wide[value_name_chars] = {};
    size_t length = 0u;
    for (; name[length] != '\0' && length + 1u < value_name_chars; length++)
        wide[length] = static_cast<WCHAR>(name[length]);
    UNICODE_STRING value_name = {static_cast<USHORT>(length * sizeof(WCHAR)), static_cast<USHORT>(sizeof wide), wide};
    HANDLE key = nullptr;
    if (!NT_SUCCESS(IoOpenDeviceRegistryKey(static_cast<PDEVICE_OBJECT>(physical_device_object),
                                            PLUGPLAY_REGKEY_DRIVER, KEY_SET_VALUE, &key)))
        return Status::not_found;
    const NTSTATUS written = ZwSetValueKey(key, &value_name, 0u, type, const_cast<void *>(bytes), size);
    ZwClose(key);
    return NT_SUCCESS(written) ? Status::ok : Status::not_supported;
}

uint32_t KernelPlatform::read_service_u32(void *registry_path, const char *name, uint32_t fallback)
{
    AV_SCOPE("KernelPlatform::read_service_u32");
    WCHAR  wide[value_name_chars] = {};
    size_t length = 0u;
    for (; name[length] != '\0' && length + 1u < value_name_chars; length++)
        wide[length] = static_cast<WCHAR>(name[length]);
    UNICODE_STRING    value_name = {static_cast<USHORT>(length * sizeof(WCHAR)), static_cast<USHORT>(sizeof wide), wide};
    OBJECT_ATTRIBUTES attributes = {};
    InitializeObjectAttributes(&attributes, static_cast<PUNICODE_STRING>(registry_path),
                               OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, nullptr, nullptr);
    HANDLE key = nullptr;
    if (!NT_SUCCESS(ZwOpenKey(&key, KEY_QUERY_VALUE, &attributes))) return fallback;
    constexpr ULONG value_bytes = sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(ULONG);
    alignas(8) UCHAR buffer[value_bytes] = {};
    ULONG            returned = 0u;
    const NTSTATUS   queried = ZwQueryValueKey(key, &value_name, KeyValuePartialInformation, buffer, value_bytes,
                                               &returned);
    ZwClose(key);
    const auto *const value = reinterpret_cast<const KEY_VALUE_PARTIAL_INFORMATION *>(buffer);
    if (!NT_SUCCESS(queried) || value->Type != REG_DWORD || value->DataLength != sizeof(ULONG)) return fallback;
    ULONG number = 0u;
    __builtin_memcpy(&number, value->Data, sizeof number);
    return number;
}

Status KernelPlatform::write_driver_text(void *physical_device_object, const char *name, const uint16_t *text,
                                         uint32_t bytes)
{
    AV_SCOPE("KernelPlatform::write_driver_text");
    return write_driver_value(physical_device_object, name, REG_SZ, text, bytes);
}

Status KernelPlatform::write_driver_u32(void *physical_device_object, const char *name, uint32_t value)
{
    AV_SCOPE("KernelPlatform::write_driver_u32");
    return write_driver_value(physical_device_object, name, REG_DWORD, &value, sizeof value);
}

Status KernelPlatform::write_driver_u64(void *physical_device_object, const char *name, uint64_t value)
{
    AV_SCOPE("KernelPlatform::write_driver_u64");
    return write_driver_value(physical_device_object, name, REG_QWORD, &value, sizeof value);
}

/* Broken invariants mean corrupted state; the kernel's own response to that is
 * a fail-fast, not continuing. */
void KernelPlatform::report_broken_invariant(const char *what, const char *function)
{
    AV_SCOPE("KernelPlatform::report_broken_invariant");
    DbgPrintEx(DPFLTR_IHVVIDEO_ID, DPFLTR_ERROR_LEVEL, "avgpu: invariant %s broken in %s\n",
               what, function);
    __fastfail(FAST_FAIL_INVALID_ARG);
}

}  // namespace av
