/* A HostClock the test drives: each reading moves it on by a step, as a
 * running thread's would, and a pause moves it on by the pause, at once. */
#ifndef AV_SCRIPTED_CLOCK_HPP
#define AV_SCRIPTED_CLOCK_HPP

#include "executor/host_clock.hpp"

#include <stdint.h>

namespace av {

class ScriptedClock final : public HostClock {
public:
    ScriptedClock(uint64_t start, uint64_t step);

    uint64_t now_nanoseconds() const override;
    void     pause(uint64_t nanoseconds) override;

    /* The pauses asked for, how many of them yielded (0), and their sum. */
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

}  // namespace av

#endif  // AV_SCRIPTED_CLOCK_HPP
