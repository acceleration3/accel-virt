#ifndef AV_CHANNEL_STATS_LATEST_HPP
#define AV_CHANNEL_STATS_LATEST_HPP

#include "proto/submission/channel_stats_report.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class StateWriter;

class ChannelStatsLatest {
public:
    ChannelStatsLatest();
    ChannelStatsLatest(const ChannelStatsLatest &) = delete;
    ChannelStatsLatest &operator=(const ChannelStatsLatest &) = delete;

    void accept(uint32_t context_id, const uint8_t *body, size_t size);
    void reset();
    bool received() const;
    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    uint32_t context_id_;
    uint64_t reports_;
    uint64_t rejected_;
    uint64_t values_[ChannelStatsReport::counters];
};

}

#endif
