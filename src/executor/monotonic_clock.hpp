/* The host's CLOCK_MONOTONIC: what every bounded wait in the executor measures
 * its deadline on - a poll's (PollBudget) and a condition's (`ahead`, for a
 * condition variable made on this clock). */
#ifndef AV_MONOTONIC_CLOCK_HPP
#define AV_MONOTONIC_CLOCK_HPP

#include "executor/host_clock.hpp"

#include <time.h>

namespace av {

class MonotonicClock final : public HostClock {
public:
    MonotonicClock();

    uint64_t now_nanoseconds() const override;
    void     pause(uint64_t nanoseconds) override;
    /* The time `nanoseconds` from now, as PosixCondition::wait_until takes it. */
    timespec ahead(uint64_t nanoseconds) const;

private:
    clockid_t clock_;
};

}  // namespace av

#endif  // AV_MONOTONIC_CLOCK_HPP
