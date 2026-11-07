#ifndef AV_D3D12_SUBRESOURCE_EXTENT_HPP
#define AV_D3D12_SUBRESOURCE_EXTENT_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

struct D3D12_RESOURCE_DESC;

namespace av {

class D3d12SubresourceExtent {
public:
    D3d12SubresourceExtent(const D3D12_RESOURCE_DESC &desc, uint32_t subresource);

    bool                   known() const;
    uint32_t               plane() const;
    const d3d12proto::Box &whole() const;
    bool                   holds(const d3d12proto::Box &region) const;

private:
    d3d12proto::Box whole_;
    uint32_t        block_width_;
    uint32_t        block_height_;
    uint32_t        plane_;
    bool            known_;
};

}

#endif
