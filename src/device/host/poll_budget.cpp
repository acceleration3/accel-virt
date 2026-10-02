#include "device/host/poll_budget.hpp"

#include "core/trace/function_scope.hpp"

#include <time.h>

namespace av {

PollBudget::PollBudget(uint32_t pause_nanoseconds, uint32_t most_polls)
    : pause_nanoseconds_(pause_nanoseconds), most_polls_(most_polls), polls_(0u)
{
    AV_SCOPE("PollBudget::PollBudget");
}

bool PollBudget::next()
{
    AV_SCOPE("PollBudget::next");
    if (polls_ >= most_polls_) return false;
    if (polls_ != 0u) {
        const timespec pause = {0, static_cast<long>(pause_nanoseconds_)};
        nanosleep(&pause, nullptr);
    }
    polls_++;
    return true;
}

uint32_t PollBudget::polls() const
{
    AV_SCOPE("PollBudget::polls");
    return polls_;
}

}  // namespace av
