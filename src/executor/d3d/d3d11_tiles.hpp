#ifndef AV_D3D11_TILES_HPP
#define AV_D3D11_TILES_HPP

#include "proto/generated/d3d/d3d_wire_types.h"

#include <stdint.h>

struct ID3D11Device5;
struct ID3D11DeviceContext4;

namespace av {

class D3dObjectTable;

class D3d11Tiles {
public:
    static constexpr uint32_t most_pieces = 256u;
    static constexpr uint32_t tile_bytes = 65536u;

    explicit D3d11Tiles(const D3dObjectTable &objects);
    D3d11Tiles(const D3d11Tiles &) = delete;
    D3d11Tiles &operator=(const D3d11Tiles &) = delete;

    [[nodiscard]] bool tiling(ID3D11Device5 &device, uint32_t resource, uint32_t &tiles, uint32_t &packed_mips,
                              uint32_t &packed_tiles) const;
    [[nodiscard]] bool update_mappings(ID3D11DeviceContext4 &context, uint32_t resource, uint32_t region_count,
                                       const d3dproto::TileRegion *regions, uint32_t pool, uint32_t range_count,
                                       const d3dproto::TileRange *ranges, uint32_t flags) const;
    [[nodiscard]] bool copy_mappings(ID3D11DeviceContext4 &context, uint32_t destination,
                                     const d3dproto::TileCoordinate &destination_start, uint32_t source,
                                     const d3dproto::TileCoordinate &source_start, const d3dproto::TileSize &size,
                                     uint32_t flags) const;
    [[nodiscard]] bool copy(ID3D11DeviceContext4 &context, uint32_t resource, const d3dproto::TileCoordinate &start,
                            const d3dproto::TileSize &size, uint32_t buffer, uint64_t buffer_offset,
                            uint32_t flags) const;
    [[nodiscard]] bool update(ID3D11DeviceContext4 &context, uint32_t resource, const d3dproto::TileCoordinate &start,
                              const d3dproto::TileSize &size, uint32_t flags, const uint8_t *data,
                              uint64_t data_size) const;
    [[nodiscard]] bool resize(ID3D11DeviceContext4 &context, uint32_t pool, uint64_t bytes) const;
    [[nodiscard]] bool barrier(ID3D11DeviceContext4 &context, uint32_t before, uint32_t after) const;

private:
    const D3dObjectTable &objects_;
};

}

#endif
