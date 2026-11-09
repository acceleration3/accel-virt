#include "executor/d3d/d3d12_layout_barrier.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

namespace {
constexpr uint32_t type_global = 0u;
constexpr uint32_t type_texture = 1u;
constexpr uint32_t type_buffer = 2u;
constexpr uint32_t sync_split = 0x80000000u;
constexpr uint32_t sync_known = 0x0180FFFFu;
constexpr uint32_t access_known = 0x1FFFFu;
constexpr uint32_t access_none = 0x80000000u;
constexpr uint32_t layout_undefined = 0xFFFFFFFFu;
constexpr uint32_t most_common_layout = 11u;
constexpr uint32_t first_queue_layout = 18u;
constexpr uint32_t last_queue_layout = 29u;
constexpr uint32_t legacy_copy_source = 0x80000000u;
constexpr uint32_t legacy_copy_dest = 0x80000001u;
constexpr uint32_t legacy_shader_resource = 0x80000002u;
constexpr uint32_t legacy_pixel_shader_resource = 0x80000003u;
constexpr uint32_t legacy_generic_read_compute = 31u;
constexpr uint32_t layout_generic_read = 1u;
constexpr uint32_t layout_shader_resource = 6u;
constexpr uint32_t layout_copy_source = 7u;
constexpr uint32_t layout_copy_dest = 8u;
constexpr uint32_t flag_discard = 1u;
constexpr uint32_t all_subresources = 0xFFFFFFFFu;
constexpr uint32_t most_planes = 2u;

bool sync_holds(uint32_t sync)
{
    AV_SCOPE("sync_holds");
    return (sync & ~sync_known) == 0u;
}

bool access_holds(uint32_t access)
{
    AV_SCOPE("access_holds");
    return access == access_none || (access & ~access_known) == 0u;
}

bool layout_holds(uint32_t layout)
{
    AV_SCOPE("layout_holds");
    return layout == layout_undefined || layout <= most_common_layout ||
           (layout >= first_queue_layout && layout <= last_queue_layout) ||
           (layout >= legacy_copy_source && layout <= legacy_pixel_shader_resource) ||
           layout == legacy_generic_read_compute;
}

}

D3d12LayoutBarrier::D3d12LayoutBarrier(const d3d12proto::LayoutBarrier &barrier) : barrier_(barrier)
{
    AV_SCOPE("D3d12LayoutBarrier::D3d12LayoutBarrier");
}

bool D3d12LayoutBarrier::holds() const
{
    AV_SCOPE("D3d12LayoutBarrier::holds");
    const d3d12proto::LayoutBarrier &b = barrier_;
    if (b.type > type_buffer || !sync_holds(b.sync_before & ~sync_split) || !sync_holds(b.sync_after & ~sync_split) ||
        !access_holds(b.access_before) || !access_holds(b.access_after))
        return false;
    if (b.type != type_texture) return b.type == type_global || b.resource != 0u;
    return b.resource != 0u && layout_holds(b.layout_before) && layout_holds(b.layout_after) && b.flags <= flag_discard &&
           b.plane_count <= most_planes;
}

bool D3d12LayoutBarrier::fits(uint32_t dimension, uint32_t array_size, uint32_t mips, uint32_t planes) const
{
    AV_SCOPE("D3d12LayoutBarrier::fits");
    const d3d12proto::LayoutBarrier &b = barrier_;
    const uint64_t slices = dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1u : array_size;
    if (b.mip_count == 0u) return b.first_mip == all_subresources || b.first_mip < uint64_t{mips} * slices * planes;
    const uint64_t slice_count = b.slice_count == 0u ? 1u : b.slice_count;
    const uint64_t plane_count = b.plane_count == 0u ? 1u : b.plane_count;
    return uint64_t{b.first_mip} + b.mip_count <= mips && uint64_t{b.first_slice} + slice_count <= slices &&
           uint64_t{b.first_plane} + plane_count <= planes;
}

uint32_t D3d12LayoutBarrier::api_layout(uint32_t layout)
{
    AV_SCOPE("D3d12LayoutBarrier::api_layout");
    switch (layout) {
    case legacy_copy_source: return layout_copy_source;
    case legacy_copy_dest: return layout_copy_dest;
    case legacy_shader_resource:
    case legacy_pixel_shader_resource: return layout_shader_resource;
    case legacy_generic_read_compute: return layout_generic_read;
    default: return layout;
    }
}

uint32_t D3d12LayoutBarrier::planes_of(uint32_t format)
{
    AV_SCOPE("D3d12LayoutBarrier::planes_of");
    constexpr uint32_t r32g8x24_typeless = 19u;
    constexpr uint32_t d32_float_s8x24 = 20u;
    constexpr uint32_t r24g8_typeless = 44u;
    constexpr uint32_t d24_unorm_s8 = 45u;
    constexpr uint32_t first_planar_video = 103u;
    constexpr uint32_t last_planar_video = 106u;
    if (format == r32g8x24_typeless || format == d32_float_s8x24 || format == r24g8_typeless || format == d24_unorm_s8)
        return 2u;
    return format >= first_planar_video && format <= last_planar_video ? 2u : 1u;
}

}
