/* What each parameter of one root signature is - its D3D12_ROOT_PARAMETER_TYPE,
 * and a root constant's width - which an indirect argument that sets a root
 * parameter is judged against (D3d12IndirectSignature). vkd3d-proton reads the
 * parameter an argument names without checking there is one, or that it is of
 * the argument's kind, and a root signature cannot be asked what it holds, so
 * the layout is kept on the signature itself as private data: it lives and
 * goes with it. */
#ifndef AV_D3D12_ROOT_LAYOUT_HPP
#define AV_D3D12_ROOT_LAYOUT_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

struct ID3D12RootSignature;

namespace av {

class D3d12RootLayout {
public:
    static constexpr uint32_t max_parameters = 64u;

    /* No parameters: what read() fills. */
    D3d12RootLayout();
    /* The first max_parameters of `count` parameters, as the guest described them. */
    D3d12RootLayout(const d3d12proto::RootParameter *parameters, uint32_t count);

    /* Kept on `root`; false when the object will not keep it. */
    [[nodiscard]] bool attach(ID3D12RootSignature *root) const;
    /* Read back from `root`; false for one that keeps none. */
    [[nodiscard]] bool read(ID3D12RootSignature *root);

    /* Whether parameter `index` is a root constant with room for `count` of
     * its values from `offset`. */
    bool has_constants(uint32_t index, uint32_t offset, uint32_t count) const;
    /* Whether parameter `index` is a root descriptor of `type`. */
    bool has_descriptor(uint32_t index, uint32_t type) const;

private:
    static constexpr uint32_t constants_type = 1u;   /* D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS */

    /* The private data, as it is stored. */
    struct Stored {
        uint32_t count_;
        uint32_t types_[max_parameters];
        uint32_t values_[max_parameters];
    };

    Stored stored_;
};

}  // namespace av

#endif  // AV_D3D12_ROOT_LAYOUT_HPP
