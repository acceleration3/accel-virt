#ifndef DATA_CLOCK_H
#define DATA_CLOCK_H

// The clock the host's trace tables are stamped with: CLOCK_MONOTONIC in 100 ns units
// (docs/trace.md, "The clock"). A frame's age is this minus the frame's entry stamp.

#include "core/Types.h"

namespace inspector {

constexpr u64 kTicksPerSecond = 10'000'000;

u64 NowTicks();
f64 TicksToSeconds(u64 ticks);

} // namespace inspector

#endif
