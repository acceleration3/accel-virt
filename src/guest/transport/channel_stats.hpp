#ifndef AV_CHANNEL_STATS_HPP
#define AV_CHANNEL_STATS_HPP

#include "proto/submission/channel_counter.hpp"
#include "proto/submission/channel_stats_report.hpp"

#include <stdint.h>

namespace av {

class ChannelStats {
public:
    static constexpr uint64_t interval = 10000000u;

    ChannelStats();
    ChannelStats(const ChannelStats &) = delete;
    ChannelStats &operator=(const ChannelStats &) = delete;

    void               note(ChannelCounter counter, uint64_t amount);
    void               piece(uint64_t number);
    void               observe(ChannelCounter counter, uint64_t total);
    bool               due(uint64_t now) const;
    void               sent(uint64_t now);
    ChannelStatsReport snapshot() const;

private:
    uint64_t counters_[ChannelStatsReport::counters];
    uint64_t changes_;
    uint64_t reported_changes_;
    uint64_t reported_at_;
};

}

#endif
