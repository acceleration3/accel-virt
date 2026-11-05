#ifndef AV_CHANNEL_COUNTER_HPP
#define AV_CHANNEL_COUNTER_HPP

#include <stdint.h>

namespace av {

enum class ChannelCounter : uint32_t {
    split_transfers = 0u,
    split_pieces,
    blob_room_waits,
    blob_room_wait_us,
    ring_waits,
    ring_wait_us,
    fence_waits,
    fence_wait_us,
    inline_bulk,
    copied_mappings,
    slice_waits,
    refused_no_space,
    refused_no_memory,
    refused_other,
    arena_grows,
    arena_bytes,
    arena_peak_ring_bytes,
    arena_set_aside_bytes,
    arena_segments,
    count
};

}

#endif
