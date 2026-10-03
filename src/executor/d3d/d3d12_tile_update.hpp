/* One UpdateTileMappings in the host's form: the regions, already held to the
 * resource (D3d12TileSpace), and the heap ranges their tiles map to, each held
 * to the heap here - vkd3d-proton dereferences the heap of every range that
 * maps and walks a range of no tiles forever. At most as many of each as one
 * command carries (d3d12.toml), in arrays of that size refilled by each
 * update.
 *
 * The arrays are D3D12_TILED_RESOURCE_COORDINATE, D3D12_TILE_REGION_SIZE and
 * D3D12_TILE_RANGE_FLAGS as fixed-width structs - the same bytes, proven in
 * the source - because the Direct3D headers stay out of every header. */
#ifndef AV_D3D12_TILE_UPDATE_HPP
#define AV_D3D12_TILE_UPDATE_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"

#include <stdint.h>

struct ID3D12CommandQueue;
struct ID3D12Heap;
struct ID3D12Resource;

namespace av {

class D3d12TileUpdate {
public:
    static constexpr uint32_t most = 2048u;

    D3d12TileUpdate();
    D3d12TileUpdate(const D3d12TileUpdate &) = delete;
    D3d12TileUpdate &operator=(const D3d12TileUpdate &) = delete;

    /* The next update starts from no regions and no ranges. */
    void     begin();
    /* False when `most` are held already. A box's height and depth are held
     * to 16 bits by D3d12Session::tiles_hold. */
    bool     add_region(const d3d12proto::TileRegion &region);
    /* False for a range of no tiles, of a flag that is not one of
     * D3D12_TILE_RANGE_FLAGS, that maps tiles past `heap_tiles` (0 for no
     * heap) - and when `most` are held already. */
    bool     add_range(const d3d12proto::TileRange &range, uint64_t heap_tiles);
    /* Maps them on `queue`; there is at least one of each. `flags` is
     * D3D12_TILE_MAPPING_FLAGS. */
    void     apply(ID3D12CommandQueue *queue, ID3D12Resource *resource, ID3D12Heap *heap, uint32_t flags);

    uint32_t regions() const;
    uint32_t ranges() const;

private:
    struct Start {
        uint32_t x_;
        uint32_t y_;
        uint32_t z_;
        uint32_t subresource_;
    };
    struct Size {
        uint32_t tiles_;
        int32_t  use_box_;
        uint32_t width_;
        uint16_t height_;
        uint16_t depth_;
    };

    Start    starts_[most];
    Size     sizes_[most];
    int32_t  flags_[most];
    uint32_t offsets_[most];
    uint32_t counts_[most];
    uint32_t regions_;
    uint32_t ranges_;
};

}  // namespace av

#endif  // AV_D3D12_TILE_UPDATE_HPP
