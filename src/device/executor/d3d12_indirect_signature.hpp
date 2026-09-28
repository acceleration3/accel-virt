/* One command signature's arguments, judged before vkd3d-proton sees them,
 * and made. vkd3d-proton checks that there is one draw or dispatch and that it
 * comes last, that the stride holds the arguments, and that a root signature
 * comes exactly when an argument sets a root parameter. It trusts the rest:
 * an argument type this driver does not report (execute indirect tier 1.0 has
 * no rays, mesh or incrementing constants), the parameter a root argument
 * names and what that parameter is, a vertex buffer slot past the last, and a
 * constant count large enough to wrap the size it adds up. This refuses those,
 * and a stride that is not whole words or that no application needs. */
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

    D3d12IndirectSignature(const d3d12proto::IndirectArgument *arguments, uint32_t count, uint32_t stride);

    /* Whether every argument holds, against `root` - null for no root
     * signature, which no argument that sets a root parameter holds against. */
    bool                    holds(const D3d12RootLayout *root) const;
    /* The signature, its stride kept on it (D3d12IndirectStride), with a
     * reference the caller owns; null when the device refuses it or will not
     * keep the stride. Only for arguments that hold. */
    ID3D12CommandSignature *make(ID3D12Device *device, ID3D12RootSignature *root) const;

private:
    enum : uint32_t {   /* D3D12_INDIRECT_ARGUMENT_TYPE */
        draw,
        draw_indexed,
        dispatch,
        vertex_buffer_view,
        index_buffer_view,
        constant,
        constant_buffer_view,
        shader_resource_view,
        unordered_access_view,
    };
    static constexpr uint32_t vertex_buffer_slots = 32u;
    /* D3D12_ROOT_PARAMETER_TYPE_CBV; SRV and UAV follow it, as their argument types do. */
    static constexpr uint32_t root_descriptor_first = 2u;
    static constexpr uint32_t word_bytes = 4u;

    /* The bytes `argument` takes in a command; 0 for one that does not hold. */
    uint64_t bytes_of(const d3d12proto::IndirectArgument &argument, const D3d12RootLayout *root) const;

    const d3d12proto::IndirectArgument *arguments_;
    uint32_t                            count_;
    uint32_t                            stride_;
};

}  // namespace av

#endif  // AV_D3D12_INDIRECT_SIGNATURE_HPP
