#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "executor/channel_stats_latest.hpp"
#include "proto/submission/channel_stats_report.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::ChannelStatsReport report;
    const av::Status       decoded = report.decode(data, size);
    if (decoded == av::Status::ok) {
        if (size != av::ChannelStatsReport::encoded_size) __builtin_trap();
        std::uint8_t again[av::ChannelStatsReport::encoded_size] = {};
        if (report.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
        if (std::memcmp(again, data, size) != 0) __builtin_trap();
    } else {
        for (std::uint32_t index = 0u; index < av::ChannelStatsReport::counters; index++)
            if (report.value(static_cast<av::ChannelCounter>(index)) != 0u) __builtin_trap();
    }
    av::ChannelStatsLatest latest;
    latest.accept(1u, data, size);
    if (!latest.invariants_hold() || !latest.received()) __builtin_trap();
    char            text[2048] = {};
    av::StateWriter writer(text, sizeof text);
    latest.report(writer);
    if (!writer.invariants_hold()) __builtin_trap();
    return 0;
}
