#include "um/trace/windows_trace_host.hpp"

#include <windows.h>

namespace av {

WindowsTraceHost::WindowsTraceHost() : clock_(), frequency_(0u) {}

uint64_t WindowsTraceHost::thread_key() const
{
    return GetCurrentThreadId();
}

uint32_t WindowsTraceHost::thread_number() const
{
    return GetCurrentThreadId();
}

uint64_t WindowsTraceHost::clock() const
{
    if (frequency_ == 0u) return clock_.now();
    LARGE_INTEGER counter = {};
    QueryPerformanceCounter(&counter);
    const uint64_t ticks = static_cast<uint64_t>(counter.QuadPart);
    constexpr uint64_t hundred_ns = 10000000u;
    return ticks / frequency_ * hundred_ns + ticks % frequency_ * hundred_ns / frequency_;
}

void WindowsTraceHost::precise()
{
    LARGE_INTEGER frequency = {};
    QueryPerformanceFrequency(&frequency);
    frequency_ = static_cast<uint64_t>(frequency.QuadPart);
}

uint16_t WindowsTraceHost::level() const
{
    return 0u;
}

bool WindowsTraceHost::exchange(uint64_t &word, uint64_t expected, uint64_t desired)
{
    const auto target = reinterpret_cast<LONG64 *>(&word);
    return static_cast<uint64_t>(_InterlockedCompareExchange64(target, static_cast<LONG64>(desired),
                                                               static_cast<LONG64>(expected))) == expected;
}

}  // namespace av
