/* The platform, as an injected object rather than a set of free functions.
 *
 * Two implementations exist, which is why the abstraction is not speculative:
 * KernelPlatform (src/km), which translates each call into one kernel call, and
 * MockPlatform (tests/harness), which enforces the rules below in every unit
 * test. Every system class takes a Platform& at construction; nothing reaches
 * for a global. */
#ifndef AV_PLATFORM_HPP
#define AV_PLATFORM_HPP

#include "core/platform/block_source.hpp"
#include "core/platform/lock_rank.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class GuestPageList;

class DdiTable;


/* What the kernel calls while it bugchecks: the buffer registered with it and
 * that buffer's size. The signature is KBUGCHECK_CALLBACK_ROUTINE's. */
using BugcheckRoutine = void (*)(void *buffer, uint32_t bytes);

class Platform : public BlockSource {
public:
    Platform() = default;
    Platform(const Platform &) = delete;
    Platform &operator=(const Platform &) = delete;

    /* Physically contiguous memory a device can address; never at DISPATCH.
     * Not zero-filled. Released with the same size. */
    virtual void    *allocate_contiguous(size_t bytes) = 0;
    virtual void     release_contiguous(void *block, size_t bytes) = 0;
    /* Of a block allocate_contiguous returned; never of null. */
    virtual uint64_t physical_address(void *block) = 0;

    /* `bytes` (whole pages, non-zero) of guest RAM the driver alone owns,
     * zero-filled and cached: pages no memory manager pages out or moves until
     * release_pages, so a device may keep their addresses for as long as the
     * driver holds them - which no process's allocation promises, since VidMm
     * evicts those whenever it likes (docs/protocol.md, "Context memory").
     * Their record, which read_page_frames reads, or null when the memory is
     * not there. PASSIVE. */
    virtual void    *allocate_pages(uint64_t bytes) = 0;
    /* Of a record allocate_pages returned, with no view of it left mapped;
     * never of null. PASSIVE. */
    virtual void     release_pages(void *pages) = 0;

    /* The physical addresses of `count` pages of the MDL `mdl`, from its page
     * `first`, into `frames`. The MDL comes from dxgkrnl, so its extent is
     * checked: invalid_parameter unless it covers those pages. Never at
     * DISPATCH. */
    [[nodiscard]] virtual Status read_page_frames(void *mdl, uint32_t first, uint32_t count,
                                                  uint64_t *frames) = 0;

    /* One page of guest memory at physical address `frame` (page-aligned),
     * mapped into kernel space: how the driver reads a page a process's
     * allocation owns - a context's fence page and ring header - where it only
     * knows the frame (feature 043). Null when the platform cannot map it.
     * Never at DISPATCH. */
    virtual uint8_t *map_frame(uint64_t frame) = 0;
    /* Of a view map_frame returned; never of null. Never at DISPATCH. */
    virtual void     unmap_frame(uint8_t *view) = 0;
    /* Every page of `pages` mapped into kernel space as one view, in their
     * order: how a present's blit reaches a surface whose frames are scattered
     * with one mapping, not one per page - every unmapping is a TLB flush on
     * each processor, and a 1280x800 frame was thousands of them. Null, and
     * `mapping` null, when the platform cannot map them. Never at DISPATCH. */
    virtual uint8_t *map_pages(const GuestPageList &pages, void *&mapping) = 0;
    /* Of a view map_pages returned, with the mapping it answered. */
    virtual void     unmap_pages(uint8_t *view, void *mapping) = 0;
    /* `bytes` of device memory at physical address `physical` (both
     * page-aligned) mapped write-back into the calling process: its address
     * there in `view`, and what unmap_from_process takes in `mapping`.
     * no_memory when it cannot be mapped. Passive only. */
    [[nodiscard]] virtual Status map_to_process(uint64_t physical, uint64_t bytes, uint64_t &view,
                                                void *&mapping) = 0;
    /* Every page of `pages`, a record allocate_pages returned, mapped cached
     * into the calling process as map_to_process maps device memory, with the
     * same answers. */
    [[nodiscard]] virtual Status map_pages_to_process(void *pages, uint64_t &view, void *&mapping) = 0;
    /* The mapping gone from the process it was made in, whichever process
     * calls - or nothing more than the bookkeeping when that process has
     * exited, which took its mappings with it. Passive only. */
    virtual void                 unmap_from_process(void *mapping) = 0;
    virtual void zero(void *dst, size_t bytes) = 0;
    virtual void copy(void *dst, const void *src, size_t bytes) = 0;

    virtual Irql current_irql() const = 0;

    /* Raises to DISPATCH and returns the level to restore. */
    virtual Irql acquire_spin_lock(uintptr_t &word, LockRank rank) = 0;
    virtual void release_spin_lock(uintptr_t &word, LockRank rank, Irql previous) = 0;

    /* The lock if it is free, without waiting; whether it was taken. A taken
     * spin lock raised to DISPATCH and `previous` is the level its release
     * restores; one not taken left the level as it was. */
    [[nodiscard]] virtual bool try_acquire_spin_lock(uintptr_t &word, LockRank rank, Irql &previous) = 0;

    /* A blocking mutex in opaque storage of at least Mutex::storage_bytes. */
    virtual void initialize_mutex(void *storage) = 0;
    virtual void acquire_mutex(void *storage, LockRank rank) = 0;
    virtual void release_mutex(void *storage, LockRank rank) = 0;
    /* The mutex if it is free, without waiting; whether it was taken. */
    [[nodiscard]] virtual bool try_acquire_mutex(void *storage, LockRank rank) = 0;
    /* A mutex held across waits at PASSIVE, in storage of at least
     * WaitMutex::storage_bytes: holding it does not raise the IRQL. */
    virtual void initialize_wait_mutex(void *storage) = 0;
    virtual void acquire_wait_mutex(void *storage, LockRank rank) = 0;
    virtual void release_wait_mutex(void *storage, LockRank rank) = 0;
    [[nodiscard]] virtual bool try_acquire_wait_mutex(void *storage, LockRank rank) = 0;

    /* Monotonic, in 100 ns units. */
    virtual uint64_t interrupt_time() const = 0;
    /* InterlockedExchange64, and a read no concurrent store can tear. Any IRQL. */
    virtual void     store_atomic(uint64_t &target, uint64_t value) = 0;
    virtual uint64_t load_atomic(uint64_t &source) = 0;
    /* KeQueryPerformanceCounter: the count, and its ticks per second. Any IRQL. */
    virtual uint64_t performance_counter(uint64_t &frequency) const = 0;
    /* Busy-waits; legal at any IRQL, so keep it short. */
    virtual void     stall(uint32_t microseconds) = 0;

    /* Registers of a device BAR mapped with DxgkCbMapMemory; legal at any
     * IRQL. The caller keeps each access inside the mapping. */
    virtual uint8_t  read_register_u8(void *address) = 0;
    virtual uint16_t read_register_u16(void *address) = 0;
    virtual uint32_t read_register_u32(void *address) = 0;
    virtual void     write_register_u8(void *address, uint8_t value) = 0;
    virtual void     write_register_u16(void *address, uint16_t value) = 0;
    virtual void     write_register_u32(void *address, uint32_t value) = 0;
    virtual void     write_register_u64(void *address, uint64_t value) = 0;

    virtual void log(const char *message) = 0;
    /* The same line while the system bugchecks: no lock, no allocation, no
     * debugger print - only the debug port. */
    virtual void log_at_bugcheck(const char *message) = 0;

    /* `routine` is called with `buffer` if the system bugchecks, and the
     * buffer's `bytes` are saved in the crash dump; whether the kernel took
     * the registration. One registration at a time; PASSIVE. */
    [[nodiscard]] virtual bool register_bugcheck(void *buffer, size_t bytes, BugcheckRoutine routine) = 0;
    /* Of the registration made; nothing without one. PASSIVE. */
    virtual void deregister_bugcheck() = 0;

    /* The device's DEVPKEY_Device_DeviceDesc, kept across boots: `text` is
     * UTF-16 and `bytes` counts its NUL. PASSIVE. */
    [[nodiscard]] virtual Status set_device_description(void *physical_device_object, const uint16_t *text,
                                                        uint32_t bytes) = 0;
    /* A REG_SZ value `name` in the device's driver (software) key: `text` is
     * UTF-16 and `bytes` counts its NUL. PASSIVE. */
    [[nodiscard]] virtual Status write_driver_text(void *physical_device_object, const char *name,
                                                   const uint16_t *text, uint32_t bytes) = 0;
    /* A REG_DWORD value there. PASSIVE. */
    [[nodiscard]] virtual Status write_driver_u32(void *physical_device_object, const char *name,
                                                  uint32_t value) = 0;
    /* A REG_QWORD value there. PASSIVE. */
    [[nodiscard]] virtual Status write_driver_u64(void *physical_device_object, const char *name,
                                                  uint64_t value) = 0;

    /* A REG_DWORD value `name` in the driver's service key, or `fallback`
     * when there is none. PASSIVE. */
    virtual uint32_t read_service_u32(void *registry_path, const char *name, uint32_t fallback) = 0;
    /* DxgkInitialize: hands the table to dxgkrnl; answers an NTSTATUS. */
    virtual int32_t register_miniport(void *driver_object, void *registry_path,
                                      const DdiTable &table) = 0;
    virtual void report_broken_invariant(const char *what, const char *function) = 0;

protected:
    /* Non-virtual: the kernel has no operator delete for a virtual destructor
     * to call, and no platform is ever destroyed through a base pointer. */
    ~Platform() = default;
};

}  // namespace av

#endif  // AV_PLATFORM_HPP
