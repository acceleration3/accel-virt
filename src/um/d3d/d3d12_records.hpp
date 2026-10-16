/* What the Direct3D 12 driver keeps in the blocks D3D12Core allocates for each
 * object it creates. The runtime owns that memory (pfnCalcPrivate*Size says
 * how much), so these are plain records written by assignment, never objects
 * constructed there - as for Direct3D 11 (d3d11_records.hpp).
 *
 * Every record starts with the same header: its stamp (RecordStamp), which
 * says what the block holds and the 32-bit id the host knows the object by,
 * and the device that made it - which is how a queue's or command list's slot, called with
 * nothing but the handle, finds its device. */
#ifndef AV_UM_D3D12_RECORDS_HPP
#define AV_UM_D3D12_RECORDS_HPP

#include "icd/d3d/present_count.hpp"
#include "icd/d3d/record_stamp.hpp"
#include "proto/generated/d3d12/d3d12_wire_types.h"
#include "um/d3d/d3d12_ddi.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class HostAllocation;
}  // namespace av

namespace av::d3d12 {

class D3d12Device;
class D3d12Recorder;

struct RecordHeader {
    d3d::RecordStamp stamp;
    D3d12Device   *device;
    D3d12Recorder *recorder;
};

/* A heap: its guest address range (placed buffers are found in it), and for a
 * CPU-visible one the host memory it is (docs/host-memory.md), which the
 * session keeps. */
struct HeapRecord {
    static constexpr uint32_t tag = 0x00485641u; /* "AVH" */

    RecordHeader    header;
    uint64_t        size;
    uint64_t        address;
    HostAllocation *memory;
    uint32_t     type; /* D3D12_HEAP_TYPE */
    uint32_t     flags;
    /* Whether the host has a heap of this id: a committed resource in the
     * GPU's own memory has none, only the resource. */
    uint32_t     made;
    /* The kernel allocation of the shown surface committed in it
     * (D3d12Device::show), which D3D12Core pages in and out by naming the
     * heap (D3d12Residency); 0 for every other heap. */
    D3DKMT_HANDLE allocation;
};

/* A resource and what a view or a copy needs to know of it without asking. */
struct ResourceRecord {
    static constexpr uint32_t tag = 0x00525641u; /* "AVR" */

    RecordHeader header;
    uint64_t     width;
    uint64_t     address; /* a buffer's, 0 for a texture */
    uint32_t     dimension; /* D3D12_RESOURCE_DIMENSION */
    uint32_t     height;
    uint32_t     depth_or_array;
    uint32_t     mip_levels;
    uint32_t     format;
    uint32_t     sample_count;
    uint32_t     flags; /* D3D12_RESOURCE_FLAGS */
    uint32_t     heap;  /* the heap id it was placed in, 0 for none */
    /* The heap's record and where in it the resource starts: a resource
     * placed in a heap names the heap only by the buffer the runtime made
     * over all of it, and an offset into that buffer. */
    HeapRecord  *placed_in;
    uint64_t     heap_offset;
    /* A swap chain buffer's kernel allocation (d3d::KernelSurface), which DWM
     * opens it by, and the runtime resource it was made for: 0 and null for
     * every other resource. */
    D3DKMT_HANDLE allocation;
    /* The presents of it so far: the point the last one published on its
     * surface (docs/shared-surfaces.md). */
    d3d::PresentCount presents;
    HANDLE        runtime_resource;
    /* A reserved resource - tiles with no memory of their own, mapped to
     * heaps' by the queue - and, once the host has been asked, how it tiled
     * it: every tile, and its packed mips and their tiles. */
    uint32_t      reserved;
    uint32_t      tiling_known;
    uint32_t      tiles;
    uint32_t      packed_mips;
    uint32_t      packed_tiles;
};

/* A descriptor heap; its descriptors follow it in the block, one slot of
 * `increment` bytes each holding its own heap id and index, which is all a
 * CPU descriptor handle - the slot's address - has to say. */
struct DescriptorHeapRecord {
    static constexpr uint32_t tag = 0x00445641u; /* "AVD" */

    RecordHeader header;
    uint32_t     type; /* D3D12DDI_DESCRIPTOR_HEAP_TYPE */
    uint32_t     count;
    uint32_t     flags;
    uint32_t     increment;
};

struct DescriptorSlot {
    uint32_t heap;
    uint32_t index;
};

/* A shader: its code and its stage's signatures as the host takes them, both
 * copied into the block after this record, since the pipeline that uses them
 * comes later and the runtime's memory is gone by then. */
struct ShaderRecord {
    static constexpr uint32_t tag = 0x00535641u; /* "AVS" */

    RecordHeader header;
    uint32_t     stage; /* the SignatureEntry stage number */
    uint32_t     code_bytes;
    uint32_t     signature_count;
    uint32_t     reserved;
};

struct BlendRecord {
    static constexpr uint32_t tag = 0x00425641u; /* "AVB" */

    RecordHeader            header;
    D3D12DDI_BLEND_DESC_0010 desc;
};

struct RasterizerRecord {
    static constexpr uint32_t tag = 0x005A5641u; /* "AVZ" */

    RecordHeader                header;
    D3D12DDI_RASTERIZER_DESC_0102 desc;
};

struct DepthStencilRecord {
    static constexpr uint32_t tag = 0x00545641u; /* "AVT" */

    RecordHeader                   header;
    D3D12DDI_DEPTH_STENCIL_DESC_0095 desc;
};

struct LayoutRecord {
    static constexpr uint32_t tag = 0x004C5641u; /* "AVL" */
    static constexpr uint32_t max_elements = 32u;

    RecordHeader             header;
    uint32_t                 count;
    uint32_t                 reserved;
    d3d12proto::InputElement elements[max_elements];
};

/* Any object whose record is its header alone: a pipeline, a root signature,
 * a fence, a query heap. */
struct ObjectRecord {
    static constexpr uint32_t tag = 0x004F5641u; /* "AVO" */

    RecordHeader header;
    uint32_t     kind;
    uint32_t     reserved;
};

/* A command queue, and the kernel context D3D12Core signals and waits its
 * fences on. */
struct QueueRecord {
    static constexpr uint32_t tag = 0x00515641u; /* "AVQ" */

    RecordHeader             header;
    D3D12DDI_HRTCOMMANDQUEUE runtime;
    HANDLE                   context;
    uint32_t                 type; /* D3D12_COMMAND_LIST_TYPE */
    uint32_t                 reserved;
    uint64_t                 executed;
};

/* A command pool: the host's command allocator, made when a list first
 * records into it, since only a list says which type the allocator must be. */
struct PoolRecord {
    static constexpr uint32_t tag = 0x00505641u; /* "AVP" */

    RecordHeader header;
    uint32_t     type; /* D3D12_COMMAND_LIST_TYPE */
    uint32_t     made;
};

/* A command recorder: which pool a list reset with it records into. */
struct RecorderRecord {
    static constexpr uint32_t tag = 0x00435641u; /* "AVC" */

    RecordHeader header;
    PoolRecord  *pool;
};

/* A command list, made on the host when first reset: D3D12 at this interface
 * creates a list closed and names no allocator until the reset. */
struct ListRecord {
    static constexpr uint32_t tag = 0x00495641u; /* "AVI" */

    RecordHeader header;
    uint32_t     type; /* D3D12_COMMAND_LIST_TYPE */
    uint32_t     made;
};

}  // namespace av::d3d12

#endif  // AV_UM_D3D12_RECORDS_HPP
