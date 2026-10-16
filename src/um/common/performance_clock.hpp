/* The process's performance counter (QueryPerformanceCounter), read in its
 * own ticks or in microseconds. Its frequency is fixed at boot, so it is read
 * once, when the clock is made. Every guest driver's timing reads one: the
 * Kmt's counter the pacer and the waits run on, the Direct3D call trace, a
 * query's patience, a paging queue's. */
#ifndef AV_UM_PERFORMANCE_CLOCK_HPP
#define AV_UM_PERFORMANCE_CLOCK_HPP

#include <stdint.h>

namespace av {

class PerformanceClock {
public:
    static constexpr uint64_t microseconds_per_second = 1000000u;

    PerformanceClock();

    uint64_t ticks() const;
    /* Ticks a second; never 0. */
    uint64_t frequency() const;
    uint64_t microseconds() const;

private:
    uint64_t frequency_;
};

}  // namespace av

#endif  // AV_UM_PERFORMANCE_CLOCK_HPP
