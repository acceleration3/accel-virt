#ifndef AV_GLICD_GL_POLL_CLOCK_HPP
#define AV_GLICD_GL_POLL_CLOCK_HPP

#include "core/platform/poll_clock.hpp"

#include <stdint.h>

namespace av {
class Kmt;
}

namespace av::glicd {

class GlPollClock final : public PollClock {
public:
    GlPollClock();

    void bind(Kmt &kmt);

    uint64_t now_nanoseconds() const override;
    void     pause(uint64_t nanoseconds) override;

private:
    Kmt *kmt_;
};

}

#endif
