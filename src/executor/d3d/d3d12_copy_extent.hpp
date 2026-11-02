#ifndef AV_D3D12_COPY_EXTENT_HPP
#define AV_D3D12_COPY_EXTENT_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

struct D3D12_TEXTURE_COPY_LOCATION;

namespace av {

class D3d12CopyExtent {
public:
    D3d12CopyExtent(const D3D12_TEXTURE_COPY_LOCATION &source, const d3d12proto::Box *box);

    bool                   known() const;
    const d3d12proto::Box &read() const;
    bool                   written_at(uint32_t x, uint32_t y, uint32_t z, d3d12proto::Box &region) const;

private:
    d3d12proto::Box read_;
    bool            known_;
};

}

#endif
