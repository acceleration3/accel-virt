#ifndef AV_CHANNEL_STATS_REPORT_HPP
#define AV_CHANNEL_STATS_REPORT_HPP

#include "core/support/status.hpp"
#include "proto/submission/channel_counter.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ChannelStatsReport {
public:
    static constexpr uint32_t counters = static_cast<uint32_t>(ChannelCounter::count);
    static constexpr size_t   encoded_size = 8u + size_t{counters} * sizeof(uint64_t);

    ChannelStatsReport();

    void     set(ChannelCounter counter, uint64_t value);
    uint64_t value(ChannelCounter counter) const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t version = 2u;

    uint64_t values_[counters];
};

}

#endif
