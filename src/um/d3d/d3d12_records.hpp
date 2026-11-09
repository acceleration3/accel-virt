#ifndef AV_UM_D3D12_RECORDS_HPP
#define AV_UM_D3D12_RECORDS_HPP

#include "icd/d3d/present_count.hpp"
#include "icd/d3d/record_stamp.hpp"
#include "proto/generated/d3d12/d3d12_wire_types.h"
#include "um/d3d/d3d12_ddi.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::d3d12 {

class HeapMemory;
class D3d12Device;
class D3d12Recorder;

struct RecordHeader {
    d3d::RecordStamp stamp;
    D3d12Device   *device;
    D3d12Recorder *recorder;
};

struct HeapRecord {
    static constexpr uint32_t tag = 0x00485641u;

    RecordHeader             header;
    uint64_t                 size;
    uint64_t                 address;
    HeapMemory              *backing;
    uint32_t                 type;
    uint32_t                 flags;
    uint32_t                 made;
    uint32_t                 asked;
    uint64_t                 charged;
    D3DKMT_HANDLE            allocation;
};

struct ResourceRecord {
    static constexpr uint32_t tag = 0x00525641u;

    RecordHeader header;
    uint64_t     width;
    uint64_t     address;
    uint32_t     dimension;
    uint32_t     height;
    uint32_t     depth_or_array;
    uint32_t     mip_levels;
    uint32_t     format;
    uint32_t     sample_count;
    uint32_t     flags;
    uint32_t     heap;
    HeapRecord  *placed_in;
    uint64_t     heap_offset;
    D3DKMT_HANDLE allocation;
    d3d::PresentCount presents;
    HANDLE        runtime_resource;
    uint32_t      reserved;
    uint32_t      tiling_known;
    uint32_t      tiles;
    uint32_t      packed_mips;
    uint32_t      packed_tiles;
    uint32_t      row_pitch;
    uint32_t      surface_width;
    uint32_t      surface_height;
    uint32_t      surface_format;
};

struct DescriptorHeapRecord {
    static constexpr uint32_t tag = 0x00445641u;

    RecordHeader header;
    uint32_t     type;
    uint32_t     count;
    uint32_t     flags;
    uint32_t     increment;
};

struct DescriptorSlot {
    uint32_t heap;
    uint32_t index;
};

struct ShaderRecord {
    static constexpr uint32_t tag = 0x00535641u;

    RecordHeader header;
    uint32_t     stage;
    uint32_t     code_bytes;
    uint32_t     signature_count;
    uint32_t     stream_out_count;
    uint32_t     stride_count;
    uint32_t     rasterized_stream;
    uint32_t     strides[4];
};

struct BlendRecord {
    static constexpr uint32_t tag = 0x00425641u;

    RecordHeader            header;
    D3D12DDI_BLEND_DESC_0010 desc;
};

struct RasterizerRecord {
    static constexpr uint32_t tag = 0x005A5641u;

    RecordHeader                header;
    D3D12DDI_RASTERIZER_DESC_0102 desc;
};

struct DepthStencilRecord {
    static constexpr uint32_t tag = 0x00545641u;

    RecordHeader                   header;
    D3D12DDI_DEPTH_STENCIL_DESC_0095 desc;
};

struct LayoutRecord {
    static constexpr uint32_t tag = 0x004C5641u;
    static constexpr uint32_t max_elements = 32u;

    RecordHeader             header;
    uint32_t                 count;
    uint32_t                 reserved;
    d3d12proto::InputElement elements[max_elements];
};

enum class ShaderIo : uint32_t { standard, tessellation, mesh, none };

struct ObjectRecord {
    static constexpr uint32_t tag = 0x004F5641u;

    RecordHeader header;
    uint32_t     kind;
    uint32_t     reserved;
};

struct QueueRecord {
    static constexpr uint32_t tag = 0x00515641u;

    RecordHeader             header;
    D3D12DDI_HRTCOMMANDQUEUE runtime;
    HANDLE                   context;
    uint32_t                 type;
    uint32_t                 reserved;
    uint64_t                 executed;
};

struct PoolRecord {
    static constexpr uint32_t tag = 0x00505641u;

    RecordHeader header;
    uint32_t     type;
    uint32_t     made;
};

struct RecorderRecord {
    static constexpr uint32_t tag = 0x00435641u;

    RecordHeader header;
    PoolRecord  *pool;
};

struct ListRecord {
    static constexpr uint32_t tag = 0x00495641u;

    RecordHeader header;
    uint32_t     type;
    uint32_t     made;
};

}

#endif
