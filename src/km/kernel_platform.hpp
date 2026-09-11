/* The platform on the Windows kernel. Translation only: every member is one
 * kernel call, and none makes a decision - the rules are enforced by
 * MockPlatform in every unit test and by Driver Verifier in the guest. */
#ifndef AV_KERNEL_PLATFORM_HPP
#define AV_KERNEL_PLATFORM_HPP

#include <ntddk.h>

#include "platform.hpp"

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
    void     stall(uint32_t microseconds) override;
    void     log(const char *message) override;
    void     report_broken_invariant(const char *what, const char *function) override;

private:
    /* Indexed by Pool, so choosing a pool is a lookup rather than a branch. */
    static constexpr POOL_FLAGS pool_flags_[] = {POOL_FLAG_NON_PAGED, POOL_FLAG_PAGED};

    ULONG pool_tag_;
};

}  // namespace av

#endif  // AV_KERNEL_PLATFORM_HPP
