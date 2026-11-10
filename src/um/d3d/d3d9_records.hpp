#ifndef AV_UM_D3D9_RECORDS_HPP
#define AV_UM_D3D9_RECORDS_HPP

#include "icd/common/stream_memory.hpp"
#include "icd/d3d/present_count.hpp"
#include "um/d3d/d3d9_ddi.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::d3d9 {

struct D3d9Subresource {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    const uint8_t *system;
    uint32_t       system_pitch;
    uint32_t       system_slice_pitch;
    uint32_t       id;
    uint8_t       *shadow;
    size_t         shadow_bytes;
    uint32_t       shadow_pitch;
    uint32_t       shadow_slice_pitch;
    uint32_t         lock_depth;
    D3DDDI_LOCKFLAGS lock_flags;
    D3DDDIARG_LOCK   lock;
    icd::StreamMemory::Slice slice;
    bool                     on_slice;
    bool                     copies_only;
};

struct D3d9Resource {
    enum class Kind : uint8_t { system, texture, cube, volume, surface, vertex_buffer, index_buffer };

    Kind                 kind;
    uint32_t             id;
    D3DDDIFORMAT         format;
    D3DDDI_POOL          pool;
    D3DDDI_RESOURCEFLAGS flags;
    uint32_t             levels;
    uint32_t             count;
    D3d9Subresource     *subresources;
    HANDLE               runtime;
    D3DKMT_HANDLE        allocation;
    HANDLE               allocation_resource;
    uint32_t             allocation_pitch;
    bool                 opened;
    bool                 cross_adapter;
    uint64_t             shared_key;
    d3d::PresentCount    presents;
    uint32_t             named_point;
};

struct D3d9Query {
    uint32_t        id;
    D3DDDIQUERYTYPE type;
    uint32_t        data_bytes;
    bool            local;
};

}

#endif
