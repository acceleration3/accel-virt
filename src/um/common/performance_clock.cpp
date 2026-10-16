#include "um/common/performance_clock.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

PerformanceClock::PerformanceClock() : frequency_(1u)
{
    AV_SCOPE("PerformanceClock::PerformanceClock");
    LARGE_INTEGER frequency = {};
    QueryPerformanceFrequency(&frequency);
    if (frequency.QuadPart > 0) frequency_ = static_cast<uint64_t>(frequency.QuadPart);
}

uint64_t PerformanceClock::ticks() const
{
    AV_SCOPE("PerformanceClock::ticks");
    LARGE_INTEGER now = {};
    QueryPerformanceCounter(&now);
    return static_cast<uint64_t>(now.QuadPart);
}

uint64_t PerformanceClock::frequency() const
{
    AV_SCOPE("PerformanceClock::frequency");
    return frequency_;
}

/* Whole seconds and the ticks left over, each scaled on its own: the product
 * of the whole count and a million passes 64 bits after three weeks of uptime
 * on a 10 MHz counter. */
uint64_t PerformanceClock::microseconds() const
{
    AV_SCOPE("PerformanceClock::microseconds");
    const uint64_t now = ticks();
    return now / frequency_ * microseconds_per_second + now % frequency_ * microseconds_per_second / frequency_;
}

}  // namespace av
