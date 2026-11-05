#include "harness/executor/scripted_clock.hpp"

namespace av {

ScriptedClock::ScriptedClock(uint64_t start, uint64_t step)
    : now_(start), step_(step), pauses_(0u), yields_(0u), paused_(0u)
{
}

uint64_t ScriptedClock::now_nanoseconds() const
{
    const uint64_t now = now_;
    now_ += step_;
    return now;
}

void ScriptedClock::pause(uint64_t nanoseconds)
{
    pauses_++;
    if (nanoseconds == 0u) yields_++;
    paused_ += nanoseconds;
    now_ += nanoseconds;
}

void ScriptedClock::advance(uint64_t nanoseconds) { now_ += nanoseconds; }

uint32_t ScriptedClock::pauses() const { return pauses_; }

uint32_t ScriptedClock::yields() const { return yields_; }

uint64_t ScriptedClock::paused() const { return paused_; }

}
