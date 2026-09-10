#include "mock_platform.hpp"

#include "test_registry.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <cstdint>

namespace av {

MockPlatform::MockPlatform()
    : live_{}, live_count_(0u), allocation_sequence_(0u), fail_at_(0u),
      irql_(Irql::passive), irql_violated_(false), invariant_broken_(false),
      verbose_(std::getenv("AV_TEST_VERBOSE") != nullptr) {}

MockPlatform::~MockPlatform()
{
    if (live_count_ != 0u)
        TestRegistry::instance().record_failure(__FILE__, __LINE__,
                                                "%u allocation(s) leaked",
                                                live_count_);
    for (std::uint32_t i = 0u; i < live_count_; i++) std::free(live_[i]);
}

void *MockPlatform::allocate(std::size_t bytes, Pool)
{
    /* Allocating at or above DISPATCH_LEVEL bugchecks on Windows. Make it a
     * test failure here instead. */
    if (irql_ >= Irql::dispatch) {
        irql_violated_ = true;
        std::fprintf(stderr, "\n    allocation at IRQL %u\n",
                     static_cast<unsigned>(irql_));
        return nullptr;
    }
    allocation_sequence_++;
    if (fail_at_ != 0u && allocation_sequence_ == fail_at_) return nullptr;
    if (live_count_ == max_live) return nullptr;
    if (bytes == 0u) return nullptr;

    void *block = std::malloc(bytes);
    if (block == nullptr) return nullptr;
    /* Poison, so a caller that reads uninitialized memory is caught. */
    std::memset(block, 0xA5, bytes);
    live_[live_count_++] = block;
    return block;
}

void MockPlatform::release(void *block)
{
    if (block == nullptr) return;
    for (std::uint32_t i = 0u; i < live_count_; i++) {
        if (live_[i] == block) {
            live_[i] = live_[--live_count_];
            std::free(block);
            return;
        }
    }
    std::fprintf(stderr, "\n    release of untracked pointer %p\n", block);
    std::abort();
}

void MockPlatform::zero(void *dst, std::size_t bytes) { std::memset(dst, 0, bytes); }

void MockPlatform::copy(void *dst, const void *src, std::size_t bytes)
{
    std::memcpy(dst, src, bytes);
}

Irql MockPlatform::current_irql() const { return irql_; }

void MockPlatform::require_irql_at_most(Irql ceiling, const char *function)
{
    if (irql_ > ceiling) {
        irql_violated_ = true;
        std::fprintf(stderr,
                     "\n    IRQL violation: %s requires <= %u, called at %u\n",
                     function, static_cast<unsigned>(ceiling),
                     static_cast<unsigned>(irql_));
    }
}

void MockPlatform::log(const char *message)
{
    if (verbose_) std::fprintf(stderr, "%s\n", message);
}

void MockPlatform::report_broken_invariant(const char *what, const char *function)
{
    /* Recorded rather than fatal: a test needs to be able to prove that a
     * corrupted object is detected, and an abort would take the runner with
     * it. Nothing in src/ may rely on this being survivable. */
    invariant_broken_ = true;
    std::fprintf(stderr, "\n    invariant %s failed in %s\n", what, function);
}

void MockPlatform::fail_allocation_at(std::uint32_t nth)
{
    fail_at_ = nth;
    allocation_sequence_ = 0u;
}

void MockPlatform::set_irql(Irql irql) { irql_ = irql; }

std::uint32_t MockPlatform::live_allocations() const { return live_count_; }
std::uint32_t MockPlatform::allocations_made() const { return allocation_sequence_; }
bool MockPlatform::irql_was_violated() const   { return irql_violated_; }
bool MockPlatform::invariant_was_broken() const { return invariant_broken_; }

}  // namespace av
