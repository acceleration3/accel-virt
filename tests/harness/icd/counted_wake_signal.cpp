#include "harness/icd/counted_wake_signal.hpp"

namespace av {

CountedWakeSignal::CountedWakeSignal() : raises_(0u), waits_(0u), last_wait_(0u), pending_(false) {}

void CountedWakeSignal::raise()
{
    raises_++;
    pending_ = true;
}

bool CountedWakeSignal::wait(std::uint32_t milliseconds)
{
    waits_++;
    last_wait_ = milliseconds;
    const bool raised = pending_;
    pending_ = false;
    return raised;
}

std::uint32_t CountedWakeSignal::raises() const { return raises_; }
std::uint32_t CountedWakeSignal::waits() const { return waits_; }
std::uint32_t CountedWakeSignal::last_wait() const { return last_wait_; }

}  // namespace av
