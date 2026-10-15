/* The clock a probe times its frames by - D3DKMT's performance counter - and
 * the avgpu adapter's vertical blank period on it: what a pacing step judges
 * FIFO presentation against. */
#ifndef AV_GUEST_PROBE_VBLANK_CLOCK_HPP
#define AV_GUEST_PROBE_VBLANK_CLOCK_HPP

#include <stdint.h>

namespace av {

class Kmt;

class VblankClock {
public:
    explicit VblankClock(Kmt &kmt);

    uint64_t now() const;
    /* One vertical blank period in now()'s ticks, the mean of timed_blanks
     * timed one after another on a device of its own; zero when the adapter
     * cannot be waited on for all of them. */
    uint64_t period();

private:
    /* One blank timed alone is as long as the waiter was late to wake for
     * it: a guest descheduled for 1.7 ms of a 16.5 ms refresh measured a
     * period 9% long, and sixty paced swaps then came to 54 of it (the
     * RTX 3070 Ti's GL interval step, run final-2). Over sixteen, that
     * lateness is a sixteenth as much. */
    static constexpr uint32_t timed_blanks = 16u;

    Kmt &kmt_;
};

}  // namespace av

#endif  // AV_GUEST_PROBE_VBLANK_CLOCK_HPP
