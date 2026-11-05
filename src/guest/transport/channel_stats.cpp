#include "guest/transport/channel_stats.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ChannelStats::ChannelStats() : counters_{}, changes_(0u), reported_changes_(0u), reported_at_(0u)
{
    AV_SCOPE("ChannelStats::ChannelStats");
}

void ChannelStats::note(ChannelCounter counter, uint64_t amount)
{
    AV_SCOPE("ChannelStats::note");
    const uint32_t index = static_cast<uint32_t>(counter);
    if (index >= ChannelStatsReport::counters || amount == 0u) return;
    __atomic_fetch_add(&counters_[index], amount, __ATOMIC_RELAXED);
    __atomic_fetch_add(&changes_, 1u, __ATOMIC_RELAXED);
}

void ChannelStats::piece(uint64_t number)
{
    AV_SCOPE("ChannelStats::piece");
    if (number == 2u) note(ChannelCounter::split_transfers, 1u);
    if (number >= 2u) note(ChannelCounter::split_pieces, 1u);
}

void ChannelStats::observe(ChannelCounter counter, uint64_t total)
{
    AV_SCOPE("ChannelStats::observe");
    const uint32_t index = static_cast<uint32_t>(counter);
    if (index >= ChannelStatsReport::counters || __atomic_exchange_n(&counters_[index], total, __ATOMIC_RELAXED) == total)
        return;
    __atomic_fetch_add(&changes_, 1u, __ATOMIC_RELAXED);
}

bool ChannelStats::due(uint64_t now) const
{
    AV_SCOPE("ChannelStats::due");
    const uint64_t changes = __atomic_load_n(&changes_, __ATOMIC_RELAXED);
    return changes != reported_changes_ && (reported_at_ == 0u || now - reported_at_ >= interval);
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

}
