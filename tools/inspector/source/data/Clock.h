#ifndef DATA_CLOCK_H
#define DATA_CLOCK_H

#include "core/Types.h"

namespace inspector {

constexpr u64 kTicksPerSecond = 10'000'000;

u64 NowTicks();
f64 TicksToSeconds(u64 ticks);

}

#endif
