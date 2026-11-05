#ifndef AV_CHANNEL_COUNTER_HPP
#define AV_CHANNEL_COUNTER_HPP

#include <stdint.h>

namespace av {

enum class ChannelCounter : uint32_t {
    split_transfers = 0u,
    split_pieces,
    room_waits,
    room_wait_us,
    inline_bulk,
    copied_mappings,
    slice_waits,
    refused_no_space,
    refused_no_memory,
    refused_other,
    count
};

}

#endif
