#include "executor/monotonic_clock.hpp"

#include "core/trace/function_scope.hpp"

#include <sched.h>

namespace av {

namespace {

constexpr uint64_t nanoseconds_per_second = 1000000000u;

}  // namespace

MonotonicClock::MonotonicClock() : clock_(CLOCK_MONOTONIC)
{
    AV_SCOPE("MonotonicClock::MonotonicClock");
}

uint64_t MonotonicClock::now_nanoseconds() const
{
    AV_SCOPE("MonotonicClock::now_nanoseconds");
    timespec now = {};
    clock_gettime(clock_, &now);
    return static_cast<uint64_t>(now.tv_sec) * nanoseconds_per_second + static_cast<uint64_t>(now.tv_nsec);
}

void MonotonicClock::pause(uint64_t nanoseconds)
{
    AV_SCOPE("MonotonicClock::pause");
    if (nanoseconds == 0u) {
        sched_yield();
        return;
    }
    const timespec pause = {static_cast<time_t>(nanoseconds / nanoseconds_per_second),
                            static_cast<long>(nanoseconds % nanoseconds_per_second)};
    clock_nanosleep(clock_, 0, &pause, nullptr);
}

timespec MonotonicClock::ahead(uint64_t nanoseconds) const
{
    AV_SCOPE("MonotonicClock::ahead");
    const uint64_t at = now_nanoseconds() + nanoseconds;
    return timespec{static_cast<time_t>(at / nanoseconds_per_second), static_cast<long>(at % nanoseconds_per_second)};
}

}  // namespace av
