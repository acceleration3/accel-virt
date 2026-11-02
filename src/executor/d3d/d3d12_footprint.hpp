#ifndef AV_D3D12_FOOTPRINT_HPP
#define AV_D3D12_FOOTPRINT_HPP

#include "proto/d3d/dxgi_format_layout.hpp"
#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

namespace av {

class D3d12Footprint {
public:
    D3d12Footprint(uint32_t format, uint32_t width, uint32_t height, uint32_t depth, uint32_t row_pitch,
                   uint64_t offset);

    d3d12proto::Box whole() const;
    bool            holds(const d3d12proto::Box &region, uint64_t buffer_bytes) const;

private:
    DxgiFormatLayout layout_;
    uint32_t         width_;
    uint32_t         height_;
    uint32_t         depth_;
    uint32_t         row_pitch_;
    uint64_t         offset_;
};

}

#endif
