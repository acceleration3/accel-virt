#include "guest/transport/counter_spin.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"

namespace av {

CounterSpin::CounterSpin(const Kmt &kmt, uint64_t microseconds)
    : kmt_(kmt), microseconds_(microseconds), budget_(0u), started_(0u), running_(false)
{
    AV_SCOPE("CounterSpin::CounterSpin");
}

bool CounterSpin::lasts()
{
    AV_SCOPE("CounterSpin::lasts");
    uint64_t       frequency = 0u;
    const uint64_t now = kmt_.performance_counter(frequency);
    if (!running_) {
        running_ = true;
        started_ = now;
        budget_ = frequency / microseconds_per_second * microseconds_ +
                  frequency % microseconds_per_second * microseconds_ / microseconds_per_second;
    }
    return now - started_ < budget_;
}

/* Nothing is measured before the first ask. */
bool CounterSpin::invariants_hold() const
{
    AV_SCOPE("CounterSpin::invariants_hold");
    return running_ || (budget_ == 0u && started_ == 0u);
}

}  // namespace av
