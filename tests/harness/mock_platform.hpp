/* The adversarial platform. Not a stub: it enforces every rule the platform
 * contract states, in every unit test, and a rule broken without the test
 * claiming it fails the test when the mock is destroyed.
 *
 * One instance per test, constructed on the stack. No globals, no reset hook.
 * Tests are single-threaded, so the lock stack models one thread. */
#ifndef AV_MOCK_PLATFORM_HPP
#define AV_MOCK_PLATFORM_HPP

#include "../../src/core/platform.hpp"
#include "violation.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class MockPlatform final : public Platform {
public:
    static constexpr std::uint32_t lock_stack_depth = 16u;

    MockPlatform();
    ~MockPlatform();
    MockPlatform(const MockPlatform &) = delete;
    MockPlatform &operator=(const MockPlatform &) = delete;

    void    *allocate(std::size_t bytes, Pool pool) override;
    void     release(void *block) override;
    void     zero(void *dst, std::size_t bytes) override;
    void     copy(void *dst, const void *src, std::size_t bytes) override;
    Irql     current_irql() const override;
    void     require_irql_at_most(Irql ceiling, const char *function) override;
    Irql     acquire_spin_lock(std::uintptr_t &word, LockRank rank) override;
    void     release_spin_lock(std::uintptr_t &word, LockRank rank, Irql previous) override;
    void     initialize_mutex(void *storage) override;
    void     acquire_mutex(void *storage, LockRank rank) override;
    void     release_mutex(void *storage, LockRank rank) override;
    std::uint64_t interrupt_time() const override;
    void     log(const char *message) override;
    void     report_broken_invariant(const char *what, const char *function) override;

    /* Fail the Nth allocation from now (1-based); 0 disables. This is how
     * out-of-memory paths reach full branch coverage instead of being assumed
     * correct. */
    void fail_allocation_at(std::uint32_t nth);

    /* Simulates the IRQL a DDI is entered at. */
    void set_irql(Irql irql);

    void advance_time(std::uint64_t ticks);

    /* Removes one recorded occurrence of `kind`; false if there was none. */
    bool claim(Violation kind);

    std::uint32_t unclaimed_violations() const;
    std::uint32_t live_allocations() const;
    std::uint32_t allocations_made() const;
    std::uint32_t locks_held() const;

private:
    static constexpr std::uint32_t max_live = 4096u;
    static constexpr std::uint32_t violation_kinds =
        static_cast<std::uint32_t>(Violation::count);

    struct HeldLock {
        LockRank rank;
        Irql     previous;
    };

    void record(Violation kind, const char *format, ...);
    void push_lock(LockRank rank, Irql previous);
    bool pop_lock(LockRank rank);

    void         *live_[max_live];
    std::uint32_t live_count_;
    std::uint32_t allocation_sequence_;
    std::uint32_t fail_at_;
    Irql          irql_;
    HeldLock      held_[lock_stack_depth];
    std::uint32_t held_count_;
    std::uint32_t violations_[violation_kinds];
    std::uint64_t now_;
    bool          verbose_;
};

}  // namespace av

#endif  // AV_MOCK_PLATFORM_HPP
