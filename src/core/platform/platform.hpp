/* The platform, as an injected object rather than a set of free functions.
 *
 * Two implementations exist, which is why the abstraction is not speculative:
 * KernelPlatform (src/km), which translates each call into one kernel call, and
 * MockPlatform (tests/harness), which enforces the rules below in every unit
 * test. Every system class takes a Platform& at construction; nothing reaches
 * for a global. The contract is specs/002-platform-abstraction-and-mock/contracts/platform.md. */
#ifndef AV_PLATFORM_HPP
#define AV_PLATFORM_HPP

#include "core/platform/lock_rank.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class DdiTable;

enum class Irql : unsigned { passive = 0, apc = 1, dispatch = 2, isr = 3 };
enum class Pool : unsigned { non_paged = 0, paged = 1 };

/* What a platform timer calls at DISPATCH: the timer, and the context it was
 * created with. The signature is the kernel's EXT_CALLBACK. */
using TimerCallback = void (*)(void *timer, void *context);

class Platform {
public:
    Platform() = default;
    Platform(const Platform &) = delete;
    Platform &operator=(const Platform &) = delete;

    /* Never at DISPATCH or above, and never with zero bytes. */
    virtual void *allocate(size_t bytes, Pool pool) = 0;
    /* Never null: the kernel bugchecks on it. */
    virtual void  release(void *block) = 0;

    /* Physically contiguous memory a device can address; never at DISPATCH.
     * Not zero-filled. Released with the same size. */
    virtual void    *allocate_contiguous(size_t bytes) = 0;
    virtual void     release_contiguous(void *block, size_t bytes) = 0;
    /* Of a block allocate_contiguous returned; never of null. */
    virtual uint64_t physical_address(void *block) = 0;

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
    virtual void zero(void *dst, size_t bytes) = 0;
    virtual void copy(void *dst, const void *src, size_t bytes) = 0;

    virtual Irql current_irql() const = 0;
    virtual void require_irql_at_most(Irql ceiling, const char *function) = 0;

    /* Raises to DISPATCH and returns the level to restore. */
    virtual Irql acquire_spin_lock(uintptr_t &word, LockRank rank) = 0;
    virtual void release_spin_lock(uintptr_t &word, LockRank rank, Irql previous) = 0;

    /* A blocking mutex in opaque storage of at least Mutex::storage_bytes. */
    virtual void initialize_mutex(void *storage) = 0;
    virtual void acquire_mutex(void *storage, LockRank rank) = 0;
    virtual void release_mutex(void *storage, LockRank rank) = 0;

    /* Monotonic, in 100 ns units. */
    virtual uint64_t interrupt_time() const = 0;
    /* InterlockedExchange64, and a read no concurrent store can tear. Any IRQL. */
    virtual void     store_atomic(uint64_t &target, uint64_t value) = 0;
    virtual uint64_t load_atomic(uint64_t &source) = 0;
    /* KeQueryPerformanceCounter: the count, and its ticks per second. Any IRQL. */
    virtual uint64_t performance_counter(uint64_t &frequency) const = 0;
    /* Busy-waits; legal at any IRQL, so keep it short. */
    virtual void     stall(uint32_t microseconds) = 0;

    /* A periodic high-resolution timer, not yet running; null when out of
     * memory. Never at DISPATCH.
     *
     * While one exists the system clock is asked to tick finely: a virtual
     * machine's default 15.6 ms tick rounds a 60 Hz period up to twice it, and
     * the display's vertical blank is what DWM paces frames against (feature
     * 038, US3 - measured at 31.2 Hz on a real guest before this). */
    virtual void *create_timer(TimerCallback callback, void *context) = 0;
    /* Fires every `period` (100 ns, non-zero) from one period after the call,
     * replacing any earlier schedule. At most DISPATCH. */
    virtual void  start_timer(void *timer, uint64_t period) = 0;
    /* No new callback starts; one already running may finish. At most DISPATCH. */
    virtual void  stop_timer(void *timer) = 0;
    /* Stops, waits for a running callback, and frees. PASSIVE; never of null. */
    virtual void  destroy_timer(void *timer) = 0;

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
