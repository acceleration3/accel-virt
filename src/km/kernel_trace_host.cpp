#include "km/kernel_trace_host.hpp"

#include <ntddk.h>

namespace av {

uint64_t KernelTraceHost::thread_key() const
{
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(KeGetCurrentThread())) & key_mask;
}

uint32_t KernelTraceHost::thread_number() const
{
    return static_cast<uint32_t>(HandleToULong(PsGetCurrentThreadId()));
}

uint64_t KernelTraceHost::clock() const { return KeQueryInterruptTime(); }

uint16_t KernelTraceHost::level() const { return static_cast<uint16_t>(KeGetCurrentIrql()); }

bool KernelTraceHost::exchange(uint64_t &word, uint64_t expected, uint64_t desired)
{
    const auto target = reinterpret_cast<LONG64 *>(&word);
    return static_cast<uint64_t>(InterlockedCompareExchange64(target, static_cast<LONG64>(desired),
                                                              static_cast<LONG64>(expected))) == expected;
}

}
