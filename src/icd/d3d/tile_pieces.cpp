#include "icd/d3d/tile_pieces.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/d3d/tile_mapping_source.hpp"

namespace av::icd {

namespace {

/* D3D12_TILE_RANGE_FLAG_REUSE_SINGLE_TILE: every tile of the range is its
 * heap offset's one. */
constexpr uint32_t range_reuse = 0x4u;

}  // namespace

TilePieces::TilePieces(const TileMappingSource &source)
    : source_(source), region_(0u), range_(0u), range_used_(0u), regions_{}, ranges_{}, region_count_(0u),
      range_count_(0u), cut_short_(false)
{
    AV_SCOPE("TilePieces::TilePieces");
}

bool TilePieces::next()
{
    AV_SCOPE("TilePieces::next");
    region_count_ = 0u;
    range_count_ = 0u;
    if (cut_short_) return false;
    while (region_ < source_.regions() && range_ < source_.ranges() && region_count_ < most)
        if (!take_region()) break;
    if (region_count_ != 0u) return true;
    cut_short_ = region_ < source_.regions() && range_ < source_.ranges();
    return false;
}

bool TilePieces::take_region()
{
    AV_SCOPE("TilePieces::take_region");
    const d3d12proto::TileRegion region = source_.region(region_);
    uint32_t                     range = range_;
    uint32_t                     used = range_used_;
    uint32_t                     taken = range_count_;
    uint64_t                     left = region.size.tiles;
    while (left != 0u && range < source_.ranges()) {
        const d3d12proto::TileRange whole = source_.range(range);
        if (whole.tiles == 0u) {
            range++;
            continue;
        }
        const uint64_t rest = whole.tiles - used;
        if (taken == most) return false;
        const uint64_t take = rest < left ? rest : left;
        const uint32_t offset = whole.flags == range_reuse ? whole.heap_offset : whole.heap_offset + used;
        ranges_[taken++] = d3d12proto::TileRange{whole.flags, offset, static_cast<uint32_t>(take)};
        left -= take;
        used = take == rest ? 0u : used + static_cast<uint32_t>(take);
        if (used == 0u) range++;
    }
    regions_[region_count_++] = region;
    range_count_ = taken;
    region_++;
    range_ = range;
    range_used_ = used;
    return true;
}

const d3d12proto::TileRegion *TilePieces::regions() const
{
    AV_SCOPE("TilePieces::regions");
    return regions_;
}

uint32_t TilePieces::region_count() const
{
    AV_SCOPE("TilePieces::region_count");
    return region_count_;
}

const d3d12proto::TileRange *TilePieces::ranges() const
{
    AV_SCOPE("TilePieces::ranges");
    return ranges_;
}

uint32_t TilePieces::range_count() const
{
    AV_SCOPE("TilePieces::range_count");
    return range_count_;
}

bool TilePieces::cut_short() const
{
    AV_SCOPE("TilePieces::cut_short");
    return cut_short_;
}

}  // namespace av::icd
