#ifndef AV_UM_D3D12_GEOMETRY_WIRE_HPP
#define AV_UM_D3D12_GEOMETRY_WIRE_HPP

#include "core/support/growable_array.hpp"
#include "proto/generated/d3d12/d3d12_wire_types.h"
#include "um/d3d/d3d12_ddi.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::d3d12 {

class D3d12GeometryWire {
public:
    static constexpr uint32_t most_geometries = 8192u;

    explicit D3d12GeometryWire(BlockSource &memory);
    D3d12GeometryWire(const D3d12GeometryWire &) = delete;
    D3d12GeometryWire &operator=(const D3d12GeometryWire &) = delete;

    [[nodiscard]] bool read(const D3D12DDI_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS_0054 &inputs);

    uint32_t                              count() const;
    const d3d12proto::RaytracingGeometry *geometries() const;
    uint32_t                              layout() const;

private:
    static d3d12proto::RaytracingGeometry convert(const D3D12DDI_RAYTRACING_GEOMETRY_DESC_0054 &from);

    GrowableArray<d3d12proto::RaytracingGeometry> geometries_;
    uint32_t                                      layout_;
};

}

#endif
