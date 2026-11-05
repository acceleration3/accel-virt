#ifndef AV_SCRIPTED_CLOCK_HPP
#define AV_SCRIPTED_CLOCK_HPP

#include "core/platform/poll_clock.hpp"

#include <stdint.h>

namespace av {

class ScriptedClock final : public PollClock {
public:
    ScriptedClock(uint64_t start, uint64_t step);

    uint64_t now_nanoseconds() const override;
    void     pause(uint64_t nanoseconds) override;
    void     advance(uint64_t nanoseconds);

    uint32_t pauses() const;
    uint32_t yields() const;
    uint64_t paused() const;

private:
    mutable uint64_t now_;
    uint64_t         step_;
    uint32_t         pauses_;
    uint32_t         yields_;
    uint64_t         paused_;
};

}

#endif
