#include "icd/common/sliced_wait.hpp"

#include "core/trace/function_scope.hpp"

namespace av::icd {

namespace {

constexpr uint64_t ns_per_second = 1000000000u;
constexpr uint64_t saturated = ~uint64_t{0};

}

SlicedWait::SlicedWait(uint64_t timeout, uint64_t now, uint64_t frequency)
    : timeout_(timeout), start_(now), frequency_(frequency), slice_(0u), asked_(false)
    {
        AV_SCOPE("SlicedWait::SlicedWait");
    }

bool SlicedWait::next(uint64_t now, uint64_t &slice)
{
    AV_SCOPE("SlicedWait::next");
    uint64_t remaining = timeout_;
    if (asked_ && timeout_ != forever) {
        if (frequency_ == 0u) return false;
        const uint64_t elapsed = elapsed_ns(now);
        if (elapsed >= timeout_) return false;
        remaining = timeout_ - elapsed;
    }
    slice_ = remaining < slice_ns ? remaining : slice_ns;
    asked_ = true;
    slice = slice_;
    return true;
}

uint64_t SlicedWait::elapsed_ns(uint64_t now) const
{
    AV_SCOPE("SlicedWait::elapsed_ns");
    const uint64_t ticks = now > start_ ? now - start_ : 0u;
    uint64_t       whole = 0u;
    uint64_t       part = 0u;
    uint64_t       elapsed = 0u;
    if (__builtin_mul_overflow(ticks / frequency_, ns_per_second, &whole)) return saturated;
    if (__builtin_mul_overflow(ticks % frequency_, ns_per_second, &part)) return saturated;
    if (__builtin_add_overflow(whole, part / frequency_, &elapsed)) return saturated;
    return elapsed;
}

bool SlicedWait::invariants_hold() const
{
    AV_SCOPE("SlicedWait::invariants_hold");
    if (!asked_) return slice_ == 0u;
    return slice_ <= slice_ns && slice_ <= timeout_;
}

}
