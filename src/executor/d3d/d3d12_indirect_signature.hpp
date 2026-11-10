#ifndef AV_D3D12_INDIRECT_SIGNATURE_HPP
#define AV_D3D12_INDIRECT_SIGNATURE_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

struct ID3D12CommandSignature;
struct ID3D12Device;
struct ID3D12RootSignature;

namespace av {

class D3d12RootLayout;

class D3d12IndirectSignature {
public:
    static constexpr uint32_t max_arguments = 16u;
    static constexpr uint32_t max_stride = 1u << 16u;
    static constexpr uint32_t max_vertex_slots = 16u;

    D3d12IndirectSignature(const d3d12proto::IndirectArgument *arguments, uint32_t count, uint32_t stride);

    bool                    addressed() const;
    bool                    holds(const D3d12RootLayout *root, bool mesh_shaders = false) const;
    ID3D12CommandSignature *make(ID3D12Device *device, ID3D12RootSignature *root) const;

private:
    enum : uint32_t {
        draw,
        draw_indexed,
        dispatch,
        vertex_buffer_view,
        index_buffer_view,
        constant,
        constant_buffer_view,
        shader_resource_view,
        unordered_access_view,
        dispatch_rays,
        dispatch_mesh,
    };
    static constexpr uint32_t word_bytes = 4u;
    static constexpr uint32_t root_cbv = 2u;

    uint64_t bytes_of(const d3d12proto::IndirectArgument &argument, const D3d12RootLayout *root) const;

    const d3d12proto::IndirectArgument *arguments_;
    uint32_t                            count_;
    uint32_t                            stride_;
};

}

#endif
