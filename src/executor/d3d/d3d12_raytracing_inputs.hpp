#ifndef AV_D3D12_RAYTRACING_INPUTS_HPP
#define AV_D3D12_RAYTRACING_INPUTS_HPP

#include "core/support/growable_array.hpp"
#include "executor/shim/d3d12_raytracing_types.hpp"
#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

namespace av {

class BlockSource;
class D3d12AddressMap;

class D3d12RaytracingInputs {
public:
    static constexpr uint32_t most_geometries = 8192u;
    static constexpr uint32_t most_instances = 1u << 24u;
    static constexpr uint64_t instance_bytes = 64u;
    static constexpr uint64_t structure_alignment = 256u;

    struct Shape {
        uint32_t                              type;
        uint32_t                              flags;
        uint32_t                              count;
        uint32_t                              layout;
        uint64_t                              instances;
        const d3d12proto::RaytracingGeometry *geometries;
        uint32_t                              geometry_count;
    };

    D3d12RaytracingInputs(BlockSource &blocks, const D3d12AddressMap &addresses);
    D3d12RaytracingInputs(const D3d12RaytracingInputs &) = delete;
    D3d12RaytracingInputs &operator=(const D3d12RaytracingInputs &) = delete;

    [[nodiscard]] bool read(const Shape &shape, bool addressed);

    const d3d12new::BuildInputs &inputs() const;
    bool                         top_level() const;
    bool                         updates() const;
    bool                         compacts() const;

    static uint32_t vertex_bytes(uint32_t format);

private:
    [[nodiscard]] bool triangles(const d3d12proto::RaytracingGeometry &from, bool addressed,
                                 d3d12new::GeometryDesc &made) const;
    [[nodiscard]] bool boxes(const d3d12proto::RaytracingGeometry &from, bool addressed,
                             d3d12new::GeometryDesc &made) const;
    [[nodiscard]] bool mapped(uint64_t guest, uint64_t bytes, uint64_t alignment, bool addressed,
                              uint64_t &host) const;

    const D3d12AddressMap       &addresses_;
    GrowableArray<d3d12new::GeometryDesc> geometries_;
    d3d12new::BuildInputs        inputs_;
};

}

#endif
