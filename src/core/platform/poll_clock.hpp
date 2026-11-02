#ifndef AV_POLL_CLOCK_HPP
#define AV_POLL_CLOCK_HPP

#include <stdint.h>

namespace av {

class PollClock {
public:
    PollClock(const PollClock &) = delete;
    PollClock &operator=(const PollClock &) = delete;

    virtual uint64_t now_nanoseconds() const = 0;
    virtual void     pause(uint64_t nanoseconds) = 0;

protected:
    PollClock() = default;
    ~PollClock() = default;
};

}

#endif
