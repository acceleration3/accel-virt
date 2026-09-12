/* The platform on the Windows kernel. Translation only: every member is one
 * kernel call, and none makes a decision - the rules are enforced by
 * MockPlatform in every unit test and by Driver Verifier in the guest. */
#ifndef AV_KERNEL_PLATFORM_HPP
#define AV_KERNEL_PLATFORM_HPP

#include <ntddk.h>

#include "core/platform/platform.hpp"

namespace av {

class KernelPlatform final : public Platform {
public:
    explicit KernelPlatform(ULONG pool_tag);
    KernelPlatform(const KernelPlatform &) = delete;
    KernelPlatform &operator=(const KernelPlatform &) = delete;

    void    *allocate(size_t bytes, Pool pool) override;
    void     release(void *block) override;
    void    *allocate_contiguous(size_t bytes) override;
    void     release_contiguous(void *block, size_t bytes) override;
    uint64_t physical_address(void *block) override;
    void     zero(void *dst, size_t bytes) override;
    void     copy(void *dst, const void *src, size_t bytes) override;
    Irql     current_irql() const override;
    void     require_irql_at_most(Irql ceiling, const char *function) override;
    Irql     acquire_spin_lock(uintptr_t &word, LockRank rank) override;
    void     release_spin_lock(uintptr_t &word, LockRank rank, Irql previous) override;
    void     initialize_mutex(void *storage) override;
    void     acquire_mutex(void *storage, LockRank rank) override;
    void     release_mutex(void *storage, LockRank rank) override;
    uint64_t interrupt_time() const override;
    uint64_t performance_counter(uint64_t &frequency) const override;
    void     store_atomic(uint64_t &target, uint64_t value) override;
    uint64_t load_atomic(uint64_t &source) override;
    void    *create_timer(TimerCallback callback, void *context) override;
    void     start_timer(void *timer, uint64_t period) override;
    void     stop_timer(void *timer) override;
    void     destroy_timer(void *timer) override;
    void     stall(uint32_t microseconds) override;
    uint8_t  read_register_u8(void *address) override;
    uint16_t read_register_u16(void *address) override;
    uint32_t read_register_u32(void *address) override;
    void     write_register_u8(void *address, uint8_t value) override;
    void     write_register_u16(void *address, uint16_t value) override;
    void     write_register_u32(void *address, uint32_t value) override;
    void     write_register_u64(void *address, uint64_t value) override;
    void     log(const char *message) override;
    void     report_broken_invariant(const char *what, const char *function) override;
    int32_t  register_miniport(void *driver_object, void *registry_path,
                               const DdiTable &table) override;

private:
    /* Indexed by Pool, so choosing a pool is a lookup rather than a branch. */
    static constexpr POOL_FLAGS pool_flags_[] = {POOL_FLAG_NON_PAGED, POOL_FLAG_PAGED};
    /* QEMU isa-debugcon (docs/kb/11-implementation-plan.md, M0): one byte per
     * port write, legal at any IRQL, readable on the host from DriverEntry on. */
    static constexpr uintptr_t debug_port = 0x402u;

    ULONG      pool_tag_;
    /* One line at a time on the debug port. Four processors writing it byte by
     * byte interleave the characters of concurrent lines and truncate them,
     * which feature 037's guest log showed. Not the core SpinLock: this object
     * implements that contract, so its own logging cannot route through it. */
    KSPIN_LOCK log_lock_;
};

}  // namespace av

#endif  // AV_KERNEL_PLATFORM_HPP
