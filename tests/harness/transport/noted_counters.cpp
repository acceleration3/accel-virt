#include "harness/transport/noted_counters.hpp"

namespace av {

NotedCounters::NotedCounters() : totals_{} {}

void NotedCounters::note(ChannelCounter counter, std::uint64_t amount)
{
    const std::uint32_t index = static_cast<std::uint32_t>(counter);
    if (index < static_cast<std::uint32_t>(ChannelCounter::count)) totals_[index] += amount;
}

std::uint64_t NotedCounters::total(ChannelCounter counter) const
{
    const std::uint32_t index = static_cast<std::uint32_t>(counter);
    return index < static_cast<std::uint32_t>(ChannelCounter::count) ? totals_[index] : 0u;
}

}
