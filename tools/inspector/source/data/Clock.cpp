#include "data/Clock.h"

#include <time.h>

namespace inspector {

u64 NowTicks() {
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<u64>(now.tv_sec) * kTicksPerSecond + static_cast<u64>(now.tv_nsec) / 100;
}

f64 TicksToSeconds(u64 ticks) {
    return static_cast<f64>(ticks) / static_cast<f64>(kTicksPerSecond);
}

}
