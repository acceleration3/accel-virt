/* What the driver keeps in the blocks the Direct3D runtime allocates for each
 * object it creates - the runtime owns that memory (pfnCalcPrivate*Size says
 * how much), so these are plain records written by assignment, never objects
 * constructed there.
 *
 * Every object is named on the wire by the 32-bit id in its record, which the
 * device hands out; the host never sees a runtime handle. */
#ifndef AV_UM_D3D11_RECORDS_HPP
#define AV_UM_D3D11_RECORDS_HPP

#include "icd/d3d/swap_chain_buffer.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::d3d11 {

/* One resource. The shadow is guest memory the CPU maps: every subresource
 * packed one after the other, rows of row_bytes, for a resource the CPU
 * writes or reads (dynamic and staging usage). The allocation is the kernel's,
 * made for a resource the kernel must see - one that is presented, scanned
 * out or shared. */
struct ResourceRecord {
    static constexpr uint32_t tag = 0x52524B56u; /* "VKRR" */

    uint32_t magic;
    uint32_t id;
    uint32_t dimension;  /* D3D10DDIRESOURCE_TYPE */
    uint32_t usage;      /* D3D10_DDI_RESOURCE_USAGE */
    uint32_t bind;       /* D3D10_DDI_RESOURCE_BIND_FLAG */
    uint32_t cpu_access; /* D3D10_DDI_CPU_ACCESS */
    uint32_t misc;       /* D3D10_DDI_RESOURCE_MISC_FLAG */
    uint32_t format;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t mips;
    uint32_t layers; /* array slices; six a cube */
    uint32_t samples;
    d3d::SwapChainBuffer::Model present_model; /* how a presented buffer's frames reach the screen */
    uint32_t vidpn_source;
    uint32_t mapped_write; /* subresources written through a map since their last upload, a bit each (first 32) */
    uint32_t allocation;   /* D3DKMT_HANDLE, 0 for none */
    uint32_t allocation_pitch;
    uint32_t reserved;
    uint32_t shadow_watched; /* the shadow is WatchedShadows' write-watched region */
    uint64_t shared_key;
    void    *runtime_resource; /* D3D10DDI_HRTRESOURCE.handle */
    uint8_t *shadow;
    size_t   shadow_bytes;
};

/* A view of any kind, a shader, an input layout, a state object, a sampler. */
struct ObjectRecord {
    static constexpr uint32_t tag = 0x4F524B56u; /* "VKRO" */

    uint32_t magic;
    uint32_t id;
    uint32_t kind; /* the shader stage, for a shader */
    uint32_t resource; /* a view's resource id */
};

/* A query: its type and how many bytes its answer is. */
struct QueryRecord {
    static constexpr uint32_t tag = 0x514B5652u; /* "RVKQ" */

    uint32_t magic;
    uint32_t id;
    uint32_t type;
    uint32_t data_bytes;
    /* The session's count of the query's last end; 0 before any. */
    uint64_t last_end;
    /* When a GetData first found the host's slot behind since the last end, in
     * performance counter ticks; 0 before any. */
    uint64_t first_miss;
    /* Nonzero once an application has waited on the query: its end is sent at
     * once from then on, so the host starts on it while the frame is still
     * being recorded, as DXVK does for a query it sees stalled on. */
    uint32_t waited_on;
    uint32_t reserved;
};

/* A command list's commands, recorded by a deferred context, in memory the
 * device allocated. */
struct CommandListRecord {
    static constexpr uint32_t tag = 0x4C4B5652u; /* "RVKL" */

    uint32_t magic;
    uint32_t reserved;
    uint8_t *commands;
    size_t   length;
};

}  // namespace av::d3d11

#endif  // AV_UM_D3D11_RECORDS_HPP
