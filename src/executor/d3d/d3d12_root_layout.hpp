#ifndef AV_D3D12_ROOT_LAYOUT_HPP
#define AV_D3D12_ROOT_LAYOUT_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

struct ID3D12RootSignature;

namespace av {

class D3d12RootLayout {
public:
    static constexpr uint32_t max_parameters = 64u;

    D3d12RootLayout();
    D3d12RootLayout(const d3d12proto::RootParameter *parameters, uint32_t count);

    [[nodiscard]] bool attach(ID3D12RootSignature *root) const;
    [[nodiscard]] bool read(ID3D12RootSignature *root);

    bool has_constants(uint32_t index, uint32_t offset, uint32_t count) const;
    bool has_descriptor(uint32_t index, uint32_t type) const;

private:
    static constexpr uint32_t constants_type = 1u;

    struct Stored {
        uint32_t count_;
        uint32_t types_[max_parameters];
        uint32_t values_[max_parameters];
    };

    Stored stored_;
};

}

#endif
