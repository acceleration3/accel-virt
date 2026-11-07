#include "bench_timer.hpp"

#include <cstdio>
#include <ctime>

namespace av {

BenchTimer::BenchTimer(std::uint32_t rounds) : rounds_(rounds)
{
}

std::uint64_t BenchTimer::now()
{
    timespec at = {};
    clock_gettime(CLOCK_MONOTONIC, &at);
    return static_cast<std::uint64_t>(at.tv_sec) * 1000000000u + static_cast<std::uint64_t>(at.tv_nsec);
}

void BenchTimer::report(const char *name, std::uint64_t operations, double nanoseconds) const
{
    std::printf("%-58s %12.1f ns/op  %10.3f us/op  (%llu ops)\n", name, nanoseconds, nanoseconds / 1000.0,
                static_cast<unsigned long long>(operations));
}

}
