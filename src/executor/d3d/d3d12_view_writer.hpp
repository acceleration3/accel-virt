#ifndef AV_D3D12_VIEW_WRITER_HPP
#define AV_D3D12_VIEW_WRITER_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stddef.h>
#include <stdint.h>

struct ID3D12Device;
struct ID3D12Resource;

namespace av {

class D3d12AddressMap;
class D3dObjectTable;

class D3d12ViewWriter {
public:
    D3d12ViewWriter(const D3dObjectTable &objects, const D3d12AddressMap &addresses);

    void bind(ID3D12Device *device);

    [[nodiscard]] bool shader_resource(size_t at, bool has_desc, const d3d12proto::ViewDesc &desc) const;
    [[nodiscard]] bool unordered_access(size_t at, bool has_desc, const d3d12proto::ViewDesc &desc) const;
    [[nodiscard]] bool render_target(size_t at, bool has_desc, const d3d12proto::ViewDesc &desc) const;
    [[nodiscard]] bool depth_stencil(size_t at, bool has_desc, const d3d12proto::ViewDesc &desc) const;
    [[nodiscard]] bool constant_buffer(size_t at, uint64_t address, uint32_t size) const;
    [[nodiscard]] bool sampler(size_t at, const d3d12proto::SamplerDesc &desc) const;

private:
    ID3D12Resource *resource(uint32_t id) const;
    bool            members(void *members, uint32_t count, bool lod, const d3d12proto::ViewDesc &desc) const;

    const D3dObjectTable  &objects_;
    const D3d12AddressMap &addresses_;
    ID3D12Device          *device_;
};

}

#endif
