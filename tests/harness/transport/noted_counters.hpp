#ifndef AV_NOTED_COUNTERS_HPP
#define AV_NOTED_COUNTERS_HPP

#include "proto/submission/channel_counter.hpp"

#include <cstdint>

namespace av {

class NotedCounters {
public:
    NotedCounters();

    void          note(ChannelCounter counter, std::uint64_t amount);
    std::uint64_t total(ChannelCounter counter) const;

private:
    std::uint64_t totals_[static_cast<std::uint32_t>(ChannelCounter::count)];
};

}

#endif
