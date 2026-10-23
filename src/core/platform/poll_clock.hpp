/* Monotonic time and pauses for bounded polls in host and guest binaries. */
#ifndef AV_POLL_CLOCK_HPP
#define AV_POLL_CLOCK_HPP

#include <stdint.h>

namespace av {

class PollClock {
public:
    PollClock(const PollClock &) = delete;
    PollClock &operator=(const PollClock &) = delete;

    /* Any thread: nanoseconds since a fixed point, never going back. */
    virtual uint64_t now_nanoseconds() const = 0;
    /* The calling thread sleeps `nanoseconds`; 0 gives its core to whatever
     * else is ready to run, and returns at once if nothing is. */
    virtual void     pause(uint64_t nanoseconds) = 0;

protected:
    PollClock() = default;
    ~PollClock() = default;
};

}  // namespace av

#endif  // AV_POLL_CLOCK_HPP
