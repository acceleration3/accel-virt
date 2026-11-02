#ifndef AV_UM_PERFORMANCE_CLOCK_HPP
#define AV_UM_PERFORMANCE_CLOCK_HPP

#include "core/platform/poll_clock.hpp"

#include <stdint.h>

namespace av {

class PerformanceClock final : public PollClock {
public:
    static constexpr uint64_t microseconds_per_second = 1000000u;

    PerformanceClock();

    uint64_t ticks() const;
    uint64_t frequency() const;
    uint64_t microseconds() const;
    uint64_t now_nanoseconds() const override;
    void pause(uint64_t nanoseconds) override;

private:
    uint64_t frequency_;
};

}

#endif
