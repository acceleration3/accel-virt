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
    /* One vertical blank period in now()'s ticks, timed between two blanks on
     * a device of its own; zero when the adapter cannot be waited on. */
    uint64_t period();

private:
    Kmt &kmt_;
};

}  // namespace av

#endif  // AV_GUEST_PROBE_VBLANK_CLOCK_HPP
