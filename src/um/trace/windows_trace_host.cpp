#include "um/trace/windows_trace_host.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

WindowsTraceHost::WindowsTraceHost() : clock_()
{
    AV_SCOPE("WindowsTraceHost::WindowsTraceHost");
}

uint64_t WindowsTraceHost::thread_key() const
{
    AV_SCOPE("WindowsTraceHost::thread_key");
    return GetCurrentThreadId();
}

uint32_t WindowsTraceHost::thread_number() const
{
    AV_SCOPE("WindowsTraceHost::thread_number");
    return GetCurrentThreadId();
}

uint64_t WindowsTraceHost::clock() const
{
    AV_SCOPE("WindowsTraceHost::clock");
    return clock_.now();
}

uint16_t WindowsTraceHost::level() const
{
    AV_SCOPE("WindowsTraceHost::level");
    return 0u;
}

bool WindowsTraceHost::exchange(uint64_t &word, uint64_t expected, uint64_t desired)
{
    AV_SCOPE("WindowsTraceHost::exchange");
    const auto target = reinterpret_cast<LONG64 *>(&word);
    return static_cast<uint64_t>(_InterlockedCompareExchange64(target, static_cast<LONG64>(desired),
                                                               static_cast<LONG64>(expected))) == expected;
}

}  // namespace av
