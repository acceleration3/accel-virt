#ifndef AV_GUEST_PROBE_VBLANK_CLOCK_HPP
#define AV_GUEST_PROBE_VBLANK_CLOCK_HPP

#include <stdint.h>

namespace av {

class Kmt;

class VblankClock {
public:
    explicit VblankClock(Kmt &kmt);

    uint64_t now() const;
    uint64_t period();

private:
    static constexpr uint32_t timed_blanks = 16u;

    Kmt &kmt_;
};

}

#endif
