#include "executor/teardown_budget.hpp"

#include "core/platform/poll_clock.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

TeardownBudget::TeardownBudget(const PollClock &clock, uint64_t nanoseconds)
    : clock_(clock), deadline_(clock.now_nanoseconds() + nanoseconds)
{
    AV_SCOPE("TeardownBudget::TeardownBudget");
}

uint64_t TeardownBudget::remaining() const
{
    AV_SCOPE("TeardownBudget::remaining");
    const uint64_t now = clock_.now_nanoseconds();
    return now >= deadline_ ? 0u : deadline_ - now;
}

bool TeardownBudget::expired() const
{
    AV_SCOPE("TeardownBudget::expired");
    return remaining() == 0u;
}

}
