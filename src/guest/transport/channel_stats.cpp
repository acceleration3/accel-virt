#include "guest/transport/channel_stats.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/client/client_blob_arena.hpp"

namespace av {

ChannelStats::ChannelStats() : counters_{}, changes_(0u), reported_changes_(0u), reported_at_(0u)
{
    AV_SCOPE("ChannelStats::ChannelStats");
}

void ChannelStats::note(ChannelCounter counter, uint64_t amount)
{
    AV_SCOPE("ChannelStats::note");
    const uint32_t index = static_cast<uint32_t>(counter);
    if (index >= static_cast<uint32_t>(ChannelCounter::arena_bytes) || amount == 0u) return;
    __atomic_fetch_add(&counters_[index], amount, __ATOMIC_RELAXED);
    __atomic_fetch_add(&changes_, 1u, __ATOMIC_RELAXED);
}

void ChannelStats::piece(uint64_t number)
{
    AV_SCOPE("ChannelStats::piece");
    if (number == 2u) note(ChannelCounter::split_transfers, 1u);
    if (number >= 2u) note(ChannelCounter::split_pieces, 1u);
}

void ChannelStats::observe(const ClientBlobArena &arena)
{
    AV_SCOPE("ChannelStats::observe");
    gauge(ChannelCounter::arena_bytes, arena.lent());
    gauge(ChannelCounter::arena_peak_ring_bytes, arena.peak_ring());
    gauge(ChannelCounter::arena_set_aside_bytes, arena.set_aside_bytes());
    gauge(ChannelCounter::arena_segments, arena.segments());
}

void ChannelStats::gauge(ChannelCounter counter, uint64_t value)
{
    AV_SCOPE("ChannelStats::gauge");
    const uint32_t index = static_cast<uint32_t>(counter);
    if (index >= ChannelStatsReport::counters || __atomic_load_n(&counters_[index], __ATOMIC_RELAXED) == value) return;
    __atomic_store_n(&counters_[index], value, __ATOMIC_RELAXED);
    __atomic_fetch_add(&changes_, 1u, __ATOMIC_RELAXED);
}

bool ChannelStats::due(uint64_t now) const
{
    AV_SCOPE("ChannelStats::due");
    const uint64_t changes = __atomic_load_n(&changes_, __ATOMIC_RELAXED);
    return changes != reported_changes_ && (reported_at_ == 0u || now - reported_at_ >= interval);
}

bool ChannelStats::frame_report(uint8_t *into, size_t capacity) const
{
    AV_SCOPE("ChannelStats::frame_report");
    if (capacity < command_bytes) return false;
    return CommandHeader(CommandHeader::channel_stats_id, static_cast<uint32_t>(command_bytes), 0u)
                   .encode(into, command_bytes) == Status::ok &&
           snapshot().encode(into + CommandHeader::encoded_size, command_bytes - CommandHeader::encoded_size) ==
               Status::ok;
}

void ChannelStats::sent(uint64_t now)
{
    AV_SCOPE("ChannelStats::sent");
    reported_changes_ = __atomic_load_n(&changes_, __ATOMIC_RELAXED);
    reported_at_ = now;
}

ChannelStatsReport ChannelStats::snapshot() const
{
    AV_SCOPE("ChannelStats::snapshot");
    ChannelStatsReport report;
    for (uint32_t index = 0u; index < ChannelStatsReport::counters; index++)
        report.set(static_cast<ChannelCounter>(index), __atomic_load_n(&counters_[index], __ATOMIC_RELAXED));
    return report;
}

bool ChannelStats::invariants_hold() const
{
    AV_SCOPE("ChannelStats::invariants_hold");
    return reported_changes_ <= __atomic_load_n(&changes_, __ATOMIC_RELAXED);
}

}
