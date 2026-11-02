#ifndef AV_MONOTONIC_CLOCK_HPP
#define AV_MONOTONIC_CLOCK_HPP

#include "core/platform/poll_clock.hpp"

#include <time.h>

namespace av {

class MonotonicClock final : public PollClock {
public:
    MonotonicClock();

    uint64_t now_nanoseconds() const override;
    void     pause(uint64_t nanoseconds) override;
    timespec ahead(uint64_t nanoseconds) const;

private:
    clockid_t clock_;
};

}

#endif
