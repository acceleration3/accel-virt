#include "um/d3d/d3d12_geometry_wire.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d12 {

namespace {
constexpr uint32_t type_top = 0u;
constexpr uint32_t layout_array = 0u;
constexpr uint32_t layout_pointers = 1u;
}

D3d12GeometryWire::D3d12GeometryWire(BlockSource &memory) : geometries_(memory, Pool::paged), layout_(layout_array)
{
    AV_SCOPE("D3d12GeometryWire::D3d12GeometryWire");
}

d3d12proto::RaytracingGeometry D3d12GeometryWire::convert(const D3D12DDI_RAYTRACING_GEOMETRY_DESC_0054 &from)
{
    AV_SCOPE("D3d12GeometryWire::convert");
    d3d12proto::RaytracingGeometry to = {};
    to.type = static_cast<uint32_t>(from.Type);
    to.flags = static_cast<uint32_t>(from.Flags);
    if (from.Type == D3D12DDI_RAYTRACING_GEOMETRY_TYPE_TRIANGLES) {
        to.index_format = static_cast<uint32_t>(from.Triangles.IndexFormat);
        to.vertex_format = static_cast<uint32_t>(from.Triangles.VertexFormat);
        to.index_count = from.Triangles.IndexCount;
        to.vertex_count = from.Triangles.VertexCount;
        to.transform = from.Triangles.ColumnMajorTransform3x4;
        to.index_buffer = from.Triangles.IndexBuffer;
        to.start = from.Triangles.VertexBuffer.StartAddress;
        to.stride = from.Triangles.VertexBuffer.StrideInBytes;
    } else {
        to.count = from.AABBs.AABBCount;
        to.start = from.AABBs.AABBs.StartAddress;
        to.stride = from.AABBs.AABBs.StrideInBytes;
    }
    return to;
}

bool D3d12GeometryWire::read(const D3D12DDI_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS_0054 &inputs)
{
    AV_SCOPE("D3d12GeometryWire::read");
    geometries_.clear();
    layout_ = static_cast<uint32_t>(inputs.DescsLayout);
    if (static_cast<uint32_t>(inputs.Type) == type_top) return true;
    if (inputs.NumDescs > most_geometries || (inputs.NumDescs != 0u && inputs.pGeometryDescs == nullptr) ||
        (layout_ != layout_array && layout_ != layout_pointers))
        return false;
    if (geometries_.reserve(inputs.NumDescs) != Status::ok) return false;
    for (UINT index = 0u; index < inputs.NumDescs; index++) {
        const D3D12DDI_RAYTRACING_GEOMETRY_DESC_0054 *const each =
            layout_ == layout_array ? &inputs.pGeometryDescs[index] : inputs.ppGeometryDescs[index];
        if (each == nullptr || geometries_.append(convert(*each)) != Status::ok) return false;
    }
    layout_ = layout_array;
    return true;
}

uint32_t D3d12GeometryWire::count() const
{
    AV_SCOPE("D3d12GeometryWire::count");
    return geometries_.size();
}

const d3d12proto::RaytracingGeometry *D3d12GeometryWire::geometries() const
{
    AV_SCOPE("D3d12GeometryWire::geometries");
    return geometries_.begin();
}

uint32_t D3d12GeometryWire::layout() const
{
    AV_SCOPE("D3d12GeometryWire::layout");
    return layout_;
}

}
