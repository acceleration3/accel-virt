/* The host's monotonic time, and pauses measured on it: what a bounded poll
 * reads (PollBudget). Virtual because there are two real implementations: the
 * host's own (MonotonicClock) and the scripted one that lets a test run a
 * five-second deadline without waiting it out. */
#ifndef AV_HOST_CLOCK_HPP
#define AV_HOST_CLOCK_HPP

#include <stdint.h>

namespace av {

class HostClock {
public:
    HostClock(const HostClock &) = delete;
    HostClock &operator=(const HostClock &) = delete;

    /* Any thread: nanoseconds since a fixed point, never going back. */
    virtual uint64_t now_nanoseconds() const = 0;
    /* The calling thread sleeps `nanoseconds`; 0 gives its core to whatever
     * else is ready to run, and returns at once if nothing is. */
    virtual void     pause(uint64_t nanoseconds) = 0;

protected:
    HostClock() = default;
    ~HostClock() = default;
};

}  // namespace av

#endif  // AV_HOST_CLOCK_HPP
