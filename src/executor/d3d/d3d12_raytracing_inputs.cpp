#include "executor/d3d/d3d12_raytracing_inputs.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d12_address_map.hpp"

namespace av {

namespace {
constexpr uint32_t type_top = 0u;
constexpr uint32_t type_bottom = 1u;
constexpr uint32_t geometry_triangles = 0u;
constexpr uint32_t geometry_boxes = 1u;
constexpr uint32_t known_build_flags = 0x3Fu;
constexpr uint32_t flag_allow_compaction = 0x2u;
constexpr uint32_t flag_perform_update = 0x20u;
constexpr uint32_t known_geometry_flags = 0x3u;
constexpr uint32_t layout_array = 0u;
constexpr uint32_t most_vertices = 1u << 30u;
constexpr uint32_t most_indices = 1u << 30u;
constexpr uint64_t most_boxes = uint64_t{1} << 30u;
constexpr uint64_t transform_bytes = 48u;
constexpr uint64_t transform_alignment = 16u;
constexpr uint64_t box_bytes = 24u;
constexpr uint64_t box_alignment = 8u;
constexpr uint64_t placeholder = 256u;
constexpr uint32_t format_unknown = 0u;
constexpr uint32_t format_r32g32b32_float = 6u;
constexpr uint32_t format_r16g16b16a16_float = 10u;
constexpr uint32_t format_r32g32_float = 16u;
constexpr uint32_t format_r16g16b16a16_snorm = 13u;
constexpr uint32_t format_r16g16_float = 34u;
constexpr uint32_t format_r16g16_snorm = 37u;
constexpr uint32_t format_r32_uint = 42u;
constexpr uint32_t format_r16_uint = 57u;

uint64_t component_alignment(uint32_t format)
{
    AV_SCOPE("component_alignment");
    return format == format_r32g32b32_float || format == format_r32g32_float ? 4u : 2u;
}

uint32_t index_bytes(uint32_t format)
{
    AV_SCOPE("index_bytes");
    return format == format_r16_uint ? 2u : format == format_r32_uint ? 4u : 0u;
}

}

D3d12RaytracingInputs::D3d12RaytracingInputs(BlockSource &blocks, const D3d12AddressMap &addresses)
    : addresses_(addresses), geometries_(blocks, Pool::paged), inputs_{}
{
    AV_SCOPE("D3d12RaytracingInputs::D3d12RaytracingInputs");
}

uint32_t D3d12RaytracingInputs::vertex_bytes(uint32_t format)
{
    AV_SCOPE("D3d12RaytracingInputs::vertex_bytes");
    switch (format) {
    case format_r32g32b32_float: return 12u;
    case format_r32g32_float:
    case format_r16g16b16a16_float:
    case format_r16g16b16a16_snorm: return 8u;
    case format_r16g16_float:
    case format_r16g16_snorm: return 4u;
    default: return 0u;
    }
}

bool D3d12RaytracingInputs::mapped(uint64_t guest, uint64_t bytes, uint64_t alignment, bool addressed,
                                   uint64_t &host) const
{
    AV_SCOPE("D3d12RaytracingInputs::mapped");
    host = 0u;
    if (!addressed) {
        host = guest == 0u ? 0u : placeholder;
        return true;
    }
    if (guest == 0u || guest % alignment != 0u) return false;
    host = addresses_.translate(guest, bytes);
    return host != 0u;
}

bool D3d12RaytracingInputs::triangles(const d3d12proto::RaytracingGeometry &from, bool addressed,
                                      d3d12new::GeometryDesc &made) const
{
    AV_SCOPE("D3d12RaytracingInputs::triangles");
    const uint32_t element = vertex_bytes(from.vertex_format);
    const uint32_t index_size = index_bytes(from.index_format);
    if (element == 0u || from.vertex_count == 0u || from.vertex_count > most_vertices ||
        (from.index_format != format_unknown && index_size == 0u) || from.index_count > most_indices ||
        (addressed && (from.index_format == format_unknown) != (from.index_buffer == 0u)) ||
        (from.index_format == format_unknown && from.index_count != 0u) ||
        from.stride < element || from.stride % component_alignment(from.vertex_format) != 0u ||
        from.stride > (uint64_t{1} << 31u))
        return false;
    d3d12new::TrianglesDesc &out = made.triangles;
    out.index_format = from.index_format;
    out.vertex_format = from.vertex_format;
    out.index_count = from.index_count;
    out.vertex_count = from.vertex_count;
    out.vertices.stride = from.stride;
    const uint64_t vertex_span = from.stride * (from.vertex_count - 1u) + element;
    if (!mapped(from.start, vertex_span, component_alignment(from.vertex_format), addressed, out.vertices.start))
        return false;
    out.index_buffer = 0u;
    if (from.index_buffer != 0u &&
        !mapped(from.index_buffer, uint64_t{from.index_count} * index_size, index_size, addressed, out.index_buffer))
        return false;
    out.transform = 0u;
    return from.transform == 0u || mapped(from.transform, transform_bytes, transform_alignment, addressed, out.transform);
}

bool D3d12RaytracingInputs::boxes(const d3d12proto::RaytracingGeometry &from, bool addressed,
                                  d3d12new::GeometryDesc &made) const
{
    AV_SCOPE("D3d12RaytracingInputs::boxes");
    if (from.count > most_boxes || from.stride < box_bytes || from.stride % box_alignment != 0u ||
        from.stride > (uint64_t{1} << 31u))
        return false;
    made.aabbs.count = from.count;
    made.aabbs.boxes.stride = from.stride;
    if (from.count == 0u) {
        made.aabbs.boxes.start = 0u;
        return true;
    }
    return mapped(from.start, from.stride * (from.count - 1u) + box_bytes, box_alignment, addressed,
                  made.aabbs.boxes.start);
}

bool D3d12RaytracingInputs::read(const Shape &shape, bool addressed)
{
    AV_SCOPE("D3d12RaytracingInputs::read");
    inputs_ = d3d12new::BuildInputs{};
    geometries_.clear();
    if ((shape.type != type_top && shape.type != type_bottom) || (shape.flags & ~known_build_flags) != 0u) return false;
    inputs_.type = static_cast<int>(shape.type);
    inputs_.flags = shape.flags;
    inputs_.count = shape.count;
    inputs_.layout = static_cast<int>(layout_array);
    if (shape.type == type_top) {
        if (shape.geometry_count != 0u || shape.count > most_instances || shape.layout != layout_array) return false;
        if (shape.count == 0u) return true;
        uint64_t host = 0u;
        if (!mapped(shape.instances, uint64_t{shape.count} * instance_bytes, 16u, addressed, host)) return false;
        inputs_.instances = host;
        return true;
    }
    if (shape.geometry_count != shape.count || shape.count > most_geometries || shape.geometries == nullptr) return false;
    if (geometries_.resize(shape.count, d3d12new::GeometryDesc{}) != Status::ok) return false;
    for (uint32_t index = 0u; index < shape.count; index++) {
        const d3d12proto::RaytracingGeometry &from = shape.geometries[index];
        d3d12new::GeometryDesc               &made = geometries_.at(index);
        made = d3d12new::GeometryDesc{};
        if ((from.type != geometry_triangles && from.type != geometry_boxes) || (from.flags & ~known_geometry_flags) != 0u)
            return false;
        made.type = static_cast<int>(from.type);
        made.flags = from.flags;
        if (!(from.type == geometry_triangles ? triangles(from, addressed, made) : boxes(from, addressed, made)))
            return false;
    }
    inputs_.geometries = shape.count == 0u ? nullptr : geometries_.begin();
    return true;
}

const d3d12new::BuildInputs &D3d12RaytracingInputs::inputs() const
{
    AV_SCOPE("D3d12RaytracingInputs::inputs");
    return inputs_;
}

bool D3d12RaytracingInputs::top_level() const
{
    AV_SCOPE("D3d12RaytracingInputs::top_level");
    return inputs_.type == static_cast<int>(type_top);
}

bool D3d12RaytracingInputs::updates() const
{
    AV_SCOPE("D3d12RaytracingInputs::updates");
    return (inputs_.flags & flag_perform_update) != 0u;
}

bool D3d12RaytracingInputs::compacts() const
{
    AV_SCOPE("D3d12RaytracingInputs::compacts");
    return (inputs_.flags & flag_allow_compaction) != 0u;
}

}
