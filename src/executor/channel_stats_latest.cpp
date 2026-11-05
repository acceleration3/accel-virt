#include "executor/channel_stats_latest.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av {

ChannelStatsLatest::ChannelStatsLatest() : context_id_(0u), reports_(0u), rejected_(0u), values_{}
{
    AV_SCOPE("ChannelStatsLatest::ChannelStatsLatest");
}

void ChannelStatsLatest::accept(uint32_t context_id, const uint8_t *body, size_t size)
{
    AV_SCOPE("ChannelStatsLatest::accept");
    ChannelStatsReport report;
    if (report.decode(body, size) != Status::ok) {
        __atomic_fetch_add(&rejected_, 1u, __ATOMIC_RELAXED);
        return;
    }
    for (uint32_t index = 0u; index < ChannelStatsReport::counters; index++)
        __atomic_store_n(&values_[index], report.value(static_cast<ChannelCounter>(index)), __ATOMIC_RELAXED);
    __atomic_store_n(&context_id_, context_id, __ATOMIC_RELAXED);
    __atomic_fetch_add(&reports_, 1u, __ATOMIC_RELEASE);
}

void ChannelStatsLatest::reset()
{
    AV_SCOPE("ChannelStatsLatest::reset");
    __atomic_store_n(&reports_, 0u, __ATOMIC_RELAXED);
    __atomic_store_n(&rejected_, 0u, __ATOMIC_RELAXED);
    __atomic_store_n(&context_id_, 0u, __ATOMIC_RELAXED);
    for (uint64_t &value : values_) __atomic_store_n(&value, 0u, __ATOMIC_RELAXED);
}

bool ChannelStatsLatest::received() const
{
    AV_SCOPE("ChannelStatsLatest::received");
    return __atomic_load_n(&reports_, __ATOMIC_ACQUIRE) != 0u || __atomic_load_n(&rejected_, __ATOMIC_RELAXED) != 0u;
}

void ChannelStatsLatest::report(StateWriter &writer, uint32_t parked_blobs) const
{
    AV_SCOPE("ChannelStatsLatest::report");
    auto value = [this](ChannelCounter counter) {
        return __atomic_load_n(&values_[static_cast<uint32_t>(counter)], __ATOMIC_RELAXED);
    };
    writer.open("channel", __atomic_load_n(&context_id_, __ATOMIC_RELAXED));
    writer.number("reports", __atomic_load_n(&reports_, __ATOMIC_RELAXED));
    writer.number("rejected", __atomic_load_n(&rejected_, __ATOMIC_RELAXED));
    writer.number("split_transfers", value(ChannelCounter::split_transfers));
    writer.number("split_pieces", value(ChannelCounter::split_pieces));
    writer.number("blob_room_waits", value(ChannelCounter::blob_room_waits));
    writer.number("blob_room_wait_us", value(ChannelCounter::blob_room_wait_us));
    writer.number("ring_waits", value(ChannelCounter::ring_waits));
    writer.number("ring_wait_us", value(ChannelCounter::ring_wait_us));
    writer.number("fence_waits", value(ChannelCounter::fence_waits));
    writer.number("fence_wait_us", value(ChannelCounter::fence_wait_us));
    writer.number("inline_bulk", value(ChannelCounter::inline_bulk));
    writer.number("copied_mappings", value(ChannelCounter::copied_mappings));
    writer.number("slice_waits", value(ChannelCounter::slice_waits));
    writer.number("refused_no_space", value(ChannelCounter::refused_no_space));
    writer.number("refused_no_memory", value(ChannelCounter::refused_no_memory));
    writer.number("refused_other", value(ChannelCounter::refused_other));
    writer.number("arena_grows", value(ChannelCounter::arena_grows));
    writer.number("arena_bytes", value(ChannelCounter::arena_bytes));
    writer.number("arena_peak_ring_bytes", value(ChannelCounter::arena_peak_ring_bytes));
    writer.number("arena_set_aside_bytes", value(ChannelCounter::arena_set_aside_bytes));
    writer.number("arena_segments", value(ChannelCounter::arena_segments));
    writer.number("parked_blobs", parked_blobs);
    writer.close();
}

bool ChannelStatsLatest::invariants_hold() const
{
    AV_SCOPE("ChannelStatsLatest::invariants_hold");
    if (__atomic_load_n(&reports_, __ATOMIC_RELAXED) != 0u) return true;
    if (__atomic_load_n(&context_id_, __ATOMIC_RELAXED) != 0u) return false;
    for (const uint64_t &value : values_)
        if (__atomic_load_n(&value, __ATOMIC_RELAXED) != 0u) return false;
    return true;
}
}
