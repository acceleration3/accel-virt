#include "um/trace/windows_trace_host.hpp"

#include <windows.h>

namespace av {

WindowsTraceHost::WindowsTraceHost() : clock_() {}

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
    return clock_.now();
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
