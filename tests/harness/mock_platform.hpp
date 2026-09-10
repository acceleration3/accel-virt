/* The adversarial platform. Not a stub: it fails allocations on demand, tracks
 * leaks, and enforces the IRQL ceilings that core declares.
 *
 * One instance per test, constructed on the stack. No globals, no reset hook. */
#ifndef AV_MOCK_PLATFORM_HPP
#define AV_MOCK_PLATFORM_HPP

#include "../../src/core/platform.hpp"

namespace av {

class MockPlatform final : public Platform {
public:
    MockPlatform();
    ~MockPlatform() override;

    void *allocate(std::size_t bytes, Pool pool) override;
    void  release(void *block) override;
    void  zero(void *dst, std::size_t bytes) override;
    void  copy(void *dst, const void *src, std::size_t bytes) override;
    Irql  current_irql() const override;
    void  require_irql_at_most(Irql ceiling, const char *function) override;
    void  log(const char *message) override;
    void  report_broken_invariant(const char *what, const char *function) override;

    /* Fail the Nth allocation from now (1-based); 0 disables. This is how
     * out-of-memory paths reach full branch coverage instead of being assumed
     * correct. */
    void fail_allocation_at(std::uint32_t nth);

    void set_irql(Irql irql);

    std::uint32_t live_allocations() const;
    std::uint32_t allocations_made() const;
    bool          irql_was_violated() const;
    bool          invariant_was_broken() const;

private:
    static constexpr std::uint32_t max_live = 4096u;

    void         *live_[max_live];
    std::uint32_t live_count_;
    std::uint32_t allocation_sequence_;
    std::uint32_t fail_at_;
    Irql          irql_;
    bool          irql_violated_;
    bool          invariant_broken_;
    bool          verbose_;
};

}  // namespace av

#endif  // AV_MOCK_PLATFORM_HPP
