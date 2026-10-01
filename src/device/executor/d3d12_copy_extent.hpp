/* The texels a CopyTextureRegion reads from its source - the box when one is
 * given, else all of the source: a footprint's size, or a texture
 * subresource's mip - and the region the same size takes at the
 * destination. */
#ifndef AV_D3D12_COPY_EXTENT_HPP
#define AV_D3D12_COPY_EXTENT_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

struct D3D12_TEXTURE_COPY_LOCATION;

namespace av {

class D3d12CopyExtent {
public:
    /* `source` as the host's copy names it; `box` null for all of it. */
    D3d12CopyExtent(const D3D12_TEXTURE_COPY_LOCATION &source, const d3d12proto::Box *box);

    /* False for a subresource its texture does not have. */
    bool                   known() const;
    const d3d12proto::Box &read() const;
    /* What is read, moved to (x, y, z); false when it would pass 2^32. */
    bool                   written_at(uint32_t x, uint32_t y, uint32_t z, d3d12proto::Box &region) const;

private:
    d3d12proto::Box read_;
    bool            known_;
};

}  // namespace av

#endif  // AV_D3D12_COPY_EXTENT_HPP
