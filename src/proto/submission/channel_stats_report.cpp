#include "proto/submission/channel_stats_report.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

ChannelStatsReport::ChannelStatsReport() : values_{}
{
    AV_SCOPE("ChannelStatsReport::ChannelStatsReport");
}

void ChannelStatsReport::set(ChannelCounter counter, uint64_t value)
{
    AV_SCOPE("ChannelStatsReport::set");
    const uint32_t index = static_cast<uint32_t>(counter);
    if (index < counters) values_[index] = value;
}

uint64_t ChannelStatsReport::value(ChannelCounter counter) const
{
    AV_SCOPE("ChannelStatsReport::value");
    const uint32_t index = static_cast<uint32_t>(counter);
    return index < counters ? values_[index] : 0u;
}

Status ChannelStatsReport::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("ChannelStatsReport::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(version);
    writer.write_u32(counters);
    for (const uint64_t value : values_) writer.write_u64(value);
    return writer.status();
}

Status ChannelStatsReport::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("ChannelStatsReport::decode");
    if (bytes == nullptr || length != encoded_size) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint32_t found_version = reader.read_u32();
    const uint32_t found_counters = reader.read_u32();
    uint64_t       found[counters] = {};
    for (uint64_t &value : found) value = reader.read_u64();
    if (reader.status() != Status::ok) return reader.status();
    if (found_version != version || found_counters != counters) return Status::malformed;
    for (uint32_t index = 0u; index < counters; index++) values_[index] = found[index];
    return Status::ok;
}

}
