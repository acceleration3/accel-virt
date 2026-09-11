#include "mock_platform.hpp"

#include "test_registry.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace av {

namespace {

const char *const violation_names[] = {
    "irql_ceiling", "allocation_at_dispatch", "blocking_at_dispatch", "lock_order",
    "release_order", "lock_stack_overflow", "null_release", "broken_invariant",
    "time_overflow",
};

}  // namespace

MockPlatform::MockPlatform()
    : live_{}, live_count_(0u), allocation_sequence_(0u), fail_at_(0u),
      irql_(Irql::passive), held_{}, held_count_(0u), violations_{}, now_(0u),
      verbose_(std::getenv("AV_TEST_VERBOSE") != nullptr) {}

MockPlatform::~MockPlatform()
{
    TestRegistry &registry = TestRegistry::instance();
    for (std::uint32_t kind = 0u; kind < violation_kinds; kind++) {
        if (violations_[kind] != 0u)
            registry.record_failure(__FILE__, __LINE__, "%u unclaimed %s violation(s)",
                                    violations_[kind], violation_names[kind]);
    }
    if (live_count_ != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u allocation(s) leaked", live_count_);
    if (held_count_ != 0u)
        registry.record_failure(__FILE__, __LINE__, "%u lock(s) still held", held_count_);
    for (std::uint32_t i = 0u; i < live_count_; i++) std::free(live_[i]);
}

void MockPlatform::record(Violation kind, const char *format, ...)
{
    violations_[static_cast<std::uint32_t>(kind)]++;
    std::va_list args;
    std::fprintf(stderr, "\n    violation %s: ", violation_names[static_cast<std::uint32_t>(kind)]);
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
    std::fprintf(stderr, "\n");
}

void *MockPlatform::allocate(std::size_t bytes, Pool)
{
    /* Allocating at DISPATCH is legal for non-paged pool on Windows, but this
     * driver's submission path must never do it: make it a failure. */
    if (irql_ >= Irql::dispatch) {
        record(Violation::allocation_at_dispatch, "%zu bytes at IRQL %u", bytes,
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
    if (block == nullptr) {
        record(Violation::null_release, "ExFreePoolWithTag bugchecks on NULL");
        return;
    }
    for (std::uint32_t i = 0u; i < live_count_; i++) {
        if (live_[i] == block) {
            live_[i] = live_[--live_count_];
            std::free(block);
            return;
        }
    }
    /* Freeing memory this platform never handed out is corruption; continuing
     * would free a foreign pointer. */
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
    if (irql_ > ceiling)
        record(Violation::irql_ceiling, "%s requires <= %u, called at %u", function,
               static_cast<unsigned>(ceiling), static_cast<unsigned>(irql_));
}

void MockPlatform::push_lock(LockRank rank, Irql previous)
{
    if (held_count_ != 0u && rank <= held_[held_count_ - 1u].rank)
        record(Violation::lock_order, "rank %u acquired while holding rank %u",
               static_cast<unsigned>(rank),
               static_cast<unsigned>(held_[held_count_ - 1u].rank));
    if (held_count_ == lock_stack_depth) {
        record(Violation::lock_stack_overflow, "more than %u locks held",
               lock_stack_depth);
        return;
    }
    held_[held_count_++] = HeldLock{rank, previous};
}

bool MockPlatform::pop_lock(LockRank rank)
{
    if (held_count_ == 0u || held_[held_count_ - 1u].rank != rank) {
        record(Violation::release_order, "rank %u released but the top of the lock stack is %s",
               static_cast<unsigned>(rank), held_count_ == 0u ? "empty" : "another rank");
        return false;
    }
    held_count_--;
    return true;
}

Irql MockPlatform::acquire_spin_lock(std::uintptr_t &, LockRank rank)
{
    const Irql previous = irql_;
    push_lock(rank, previous);
    if (irql_ < Irql::dispatch) irql_ = Irql::dispatch;
    return previous;
}

void MockPlatform::release_spin_lock(std::uintptr_t &, LockRank rank, Irql previous)
{
    if (pop_lock(rank)) irql_ = previous;
}

void MockPlatform::initialize_mutex(void *) {}

void MockPlatform::acquire_mutex(void *, LockRank rank)
{
    if (irql_ >= Irql::dispatch)
        record(Violation::blocking_at_dispatch, "mutex rank %u acquired at IRQL %u",
               static_cast<unsigned>(rank), static_cast<unsigned>(irql_));
    push_lock(rank, irql_);
    if (irql_ < Irql::apc) irql_ = Irql::apc;
}

void MockPlatform::release_mutex(void *, LockRank rank)
{
    if (pop_lock(rank)) irql_ = held_[held_count_].previous;
}

std::uint64_t MockPlatform::interrupt_time() const { return now_; }

void MockPlatform::advance_time(std::uint64_t ticks)
{
    if (ticks > ~std::uint64_t{0} - now_) {
        record(Violation::time_overflow, "advancing %llu ticks from %llu",
               static_cast<unsigned long long>(ticks), static_cast<unsigned long long>(now_));
        now_ = ~std::uint64_t{0};
        return;
    }
    now_ += ticks;
}

void MockPlatform::log(const char *message)
{
    if (verbose_) std::fprintf(stderr, "%s\n", message);
}

void MockPlatform::report_broken_invariant(const char *what, const char *function)
{
    record(Violation::broken_invariant, "%s failed in %s", what, function);
}

void MockPlatform::fail_allocation_at(std::uint32_t nth)
{
    fail_at_ = nth;
    allocation_sequence_ = 0u;
}

void MockPlatform::set_irql(Irql irql) { irql_ = irql; }

bool MockPlatform::claim(Violation kind)
{
    std::uint32_t &count = violations_[static_cast<std::uint32_t>(kind)];
    if (count == 0u) return false;
    count--;
    return true;
}

std::uint32_t MockPlatform::unclaimed_violations() const
{
    std::uint32_t total = 0u;
    for (std::uint32_t kind = 0u; kind < violation_kinds; kind++) total += violations_[kind];
    return total;
}

std::uint32_t MockPlatform::live_allocations() const { return live_count_; }
std::uint32_t MockPlatform::allocations_made() const { return allocation_sequence_; }
std::uint32_t MockPlatform::locks_held() const       { return held_count_; }

}  // namespace av
