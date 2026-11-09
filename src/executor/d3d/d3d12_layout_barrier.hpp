#ifndef AV_D3D12_LAYOUT_BARRIER_HPP
#define AV_D3D12_LAYOUT_BARRIER_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

struct ID3D12Resource;

namespace av {

class D3d12LayoutBarrier {
public:
    enum class Mode : uint32_t { legacy = 1u, layout = 2u };

    explicit D3d12LayoutBarrier(const d3d12proto::LayoutBarrier &barrier);

    bool        holds() const;
    bool        fits(uint32_t dimension, uint32_t array_size, uint32_t mips, uint32_t planes) const;
    static uint32_t api_layout(uint32_t layout);
    static uint32_t planes_of(uint32_t format);
    static bool claim(ID3D12Resource *resource, Mode mode);

private:
    const d3d12proto::LayoutBarrier &barrier_;
};

}

#endif
