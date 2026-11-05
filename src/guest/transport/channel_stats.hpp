#ifndef AV_CHANNEL_STATS_HPP
#define AV_CHANNEL_STATS_HPP

#include "proto/submission/channel_counter.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/channel_stats_report.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ClientBlobArena;

class ChannelStats {
public:
    static constexpr uint64_t interval = 10000000u;
    static constexpr size_t   command_bytes = CommandHeader::encoded_size + ChannelStatsReport::encoded_size;

    ChannelStats();
    ChannelStats(const ChannelStats &) = delete;
    ChannelStats &operator=(const ChannelStats &) = delete;

    void               note(ChannelCounter counter, uint64_t amount);
    void               piece(uint64_t number);
    void               observe(const ClientBlobArena &arena);
    bool               due(uint64_t now) const;
    [[nodiscard]] bool frame_report(uint8_t *into, size_t capacity) const;
    void               sent(uint64_t now);
    ChannelStatsReport snapshot() const;

    bool invariants_hold() const;

private:
    void gauge(ChannelCounter counter, uint64_t value);

    uint64_t counters_[ChannelStatsReport::counters];
    uint64_t changes_;
    uint64_t reported_changes_;
    uint64_t reported_at_;
};

}

#endif
