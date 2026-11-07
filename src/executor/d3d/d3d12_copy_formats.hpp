#ifndef AV_D3D12_COPY_FORMATS_HPP
#define AV_D3D12_COPY_FORMATS_HPP

#include "proto/d3d/dxgi_format.hpp"
#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

namespace av {

class D3d12CopyFormats {
public:
    D3d12CopyFormats(uint32_t destination, uint32_t source);

    bool            same_texels() const;
    bool            scaled() const;
    uint32_t        destination_extent(uint32_t source_texels, bool horizontal) const;
    d3d12proto::Box rescaled(const d3d12proto::Box &written) const;
    bool            footprint_of_plane(uint32_t plane) const;

private:
    bool block_pair() const;
    bool same_layout() const;

    uint32_t   destination_format_;
    uint32_t   source_format_;
    DxgiFormat destination_;
    DxgiFormat source_;
};

}

#endif
