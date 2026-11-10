#ifndef AV_UM_D3D11_RECORDS_HPP
#define AV_UM_D3D11_RECORDS_HPP

#include "icd/common/stream_memory.hpp"
#include "icd/d3d/present_count.hpp"
#include "icd/d3d/record_stamp.hpp"
#include "icd/d3d/row_band.hpp"
#include "icd/d3d/swap_chain_buffer.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class HostAllocation;
}

namespace av::d3d11 {

struct ResourceRecord {
    static constexpr uint32_t tag = 0x52524B56u;

    d3d::RecordStamp stamp;
    uint32_t dimension;
    uint32_t usage;
    uint32_t bind;
    uint32_t cpu_access;
    uint32_t misc;
    uint32_t format;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t mips;
    uint32_t layers;
    uint32_t samples;
    d3d::SwapChainBuffer::Model present_model;
    uint32_t vidpn_source;
    uint32_t allocation;
    uint32_t accounted;
    uint32_t allocation_pitch;
    uint32_t allocation_owned;
    uint32_t refreshed;
    uint32_t shadow_watched;
    uint32_t shadow_local;
    uint64_t shared_key;
    d3d::PresentCount presents;
    d3d::RowBand pending_rows;
    void    *runtime_resource;
    uint8_t *shadow;
    size_t   shadow_bytes;
    icd::StreamMemory::Slice slice;
    uint32_t                 on_slice;
    HostAllocation          *lent;
    ResourceRecord          *immediate;
    uint64_t                 gpu_wrote;
    uint32_t                 tiles;
    uint32_t                 packed_mips;
    uint32_t                 packed_tiles;
    uint32_t                 tiling_known;
};

struct ObjectRecord {
    static constexpr uint32_t tag = 0x4F524B56u;

    d3d::RecordStamp stamp;
    uint32_t kind;
    uint32_t resource;
};

struct StreamPassthroughRecord {
    static constexpr uint32_t marker = 0x50535452u;
    static constexpr uint32_t variants = 4u;

    ObjectRecord head;
    uint32_t     variant[variants];
};

struct QueryRecord {
    static constexpr uint32_t tag = 0x00515641u;

    d3d::RecordStamp stamp;
    uint32_t type;
    uint32_t data_bytes;
    uint64_t last_end;
    uint64_t first_miss;
    uint32_t waited_on;
    uint32_t reserved;
};

struct CommandListRecord {
    static constexpr uint32_t tag = 0x004C5641u;

    d3d::RecordStamp stamp;
    uint8_t *commands;
    size_t   length;
};

}

#endif
