#include "executor/poll_budget.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/host_clock.hpp"

namespace av {

PollBudget::PollBudget(HostClock &clock, uint64_t spin_nanoseconds, uint64_t pause_nanoseconds,
                       uint64_t deadline_nanoseconds)
    : clock_(clock), spin_nanoseconds_(spin_nanoseconds), pause_nanoseconds_(pause_nanoseconds),
      deadline_nanoseconds_(deadline_nanoseconds), started_(0u), polled_(false), expired_(false)
{
    AV_SCOPE("PollBudget::PollBudget");
}

bool PollBudget::next()
{
    AV_SCOPE("PollBudget::next");
    if (!polled_) {
        started_ = clock_.now_nanoseconds();
        polled_ = true;
        return true;
    }
    if (expired_) return false;
    const uint64_t spent = clock_.now_nanoseconds() - started_;
    if (spent >= deadline_nanoseconds_) {
        expired_ = true;
        return false;
    }
    if (spent < spin_nanoseconds_) __builtin_ia32_pause();
    else clock_.pause(pause_nanoseconds_);
    return true;
}

bool PollBudget::expired() const
{
    AV_SCOPE("PollBudget::expired");
    return expired_;
}

bool PollBudget::invariants_hold() const
{
    AV_SCOPE("PollBudget::invariants_hold");
    return polled_ || (!expired_ && started_ == 0u);
}

}  // namespace av
