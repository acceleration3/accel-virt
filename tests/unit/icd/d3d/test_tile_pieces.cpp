/* An UpdateTileMappings in pieces (TilePieces): each piece whole regions and
 * the ranges their tiles take, a range running on across regions cut at each
 * region's end and its rest moved on through the heap - unless it reuses one
 * tile - and the update ending where its regions or its ranges do. A region
 * no piece can hold the ranges of stops the update there. */
#include "harness/framework/test_registry.hpp"
#include "icd/d3d/tile_mapping_source.hpp"
#include "icd/d3d/tile_pieces.hpp"

#include <cstdint>

using av::icd::TileMappingSource;
using av::icd::TilePieces;
using av::d3d12proto::TileCoordinate;
using av::d3d12proto::TileRange;
using av::d3d12proto::TileRegion;
using av::d3d12proto::TileSize;

namespace {

constexpr std::uint32_t most = TilePieces::most;
constexpr std::uint32_t range_null = 0x1u;
constexpr std::uint32_t range_reuse = 0x4u;
constexpr std::uint32_t as_many_as_are_left = 0xFFFFFFFFu;

/* An update's regions and ranges as plain arrays. */
class ScriptedSource final : public TileMappingSource {
public:
    ScriptedSource(const TileRegion *regions, std::uint32_t region_count, const TileRange *ranges,
                   std::uint32_t range_count)
        : regions_(regions), region_count_(region_count), ranges_(ranges), range_count_(range_count)
    {
    }

    std::uint32_t regions() const override { return region_count_; }
    TileRegion    region(std::uint32_t index) const override { return regions_[index]; }
    std::uint32_t ranges() const override { return range_count_; }
    TileRange     range(std::uint32_t index) const override { return ranges_[index]; }

private:
    const TileRegion *regions_;
    std::uint32_t     region_count_;
    const TileRange  *ranges_;
    std::uint32_t     range_count_;
};

TileRegion run_at(std::uint32_t x, std::uint32_t tiles)
{
    return TileRegion{TileCoordinate{x, 0u, 0u, 0u}, TileSize{tiles, 0u, 0u, 0u, 0u}};
}

bool same(const TileRange &range, std::uint32_t flags, std::uint32_t offset, std::uint32_t tiles)
{
    return range.flags == flags && range.heap_offset == offset && range.tiles == tiles;
}

TileRegion many_regions[most + 1u];
TileRange  many_ranges[most + 1u];

}  // namespace

TEST(tile_pieces, an_update_that_fits_is_one_piece_of_itself)
{ const TileRegion regions[1] = {run_at(3u, 4u)};
  const TileRange ranges[1] = {{0u, 9u, 4u}};
  const ScriptedSource source(regions, 1u, ranges, 1u);
  TilePieces pieces(source);
  CHECK(pieces.next());
  CHECK_EQ(pieces.region_count(), 1u);
  CHECK_EQ(pieces.regions()[0].start.x, 3u);
  CHECK_EQ(pieces.regions()[0].size.tiles, 4u);
  CHECK_EQ(pieces.range_count(), 1u);
  CHECK(same(pieces.ranges()[0], 0u, 9u, 4u));
  CHECK(!pieces.next());
  CHECK(!pieces.cut_short()); }

TEST(tile_pieces, a_range_across_regions_is_cut_at_each_and_moves_on_through_the_heap)
{ const TileRegion regions[3] = {run_at(0u, 3u), run_at(8u, 2u), run_at(20u, 1u)};
  const TileRange ranges[2] = {{0u, 10u, 4u}, {range_null, 0u, 2u}};
  const ScriptedSource source(regions, 3u, ranges, 2u);
  TilePieces pieces(source);
  CHECK(pieces.next());
  CHECK_EQ(pieces.region_count(), 3u);
  CHECK_EQ(pieces.range_count(), 4u);
  CHECK(same(pieces.ranges()[0], 0u, 10u, 3u));
  CHECK(same(pieces.ranges()[1], 0u, 13u, 1u));
  CHECK(same(pieces.ranges()[2], range_null, 0u, 1u));
  CHECK(same(pieces.ranges()[3], range_null, 1u, 1u)); }

TEST(tile_pieces, a_range_reusing_one_tile_keeps_it_across_the_cut)
{ const TileRegion regions[2] = {run_at(0u, 3u), run_at(5u, 2u)};
  const TileRange ranges[1] = {{range_reuse, 7u, 5u}};
  const ScriptedSource source(regions, 2u, ranges, 1u);
  TilePieces pieces(source);
  CHECK(pieces.next());
  CHECK(same(pieces.ranges()[0], range_reuse, 7u, 3u));
  CHECK(same(pieces.ranges()[1], range_reuse, 7u, 2u)); }

TEST(tile_pieces, more_regions_than_a_piece_holds_go_on_in_the_next_from_where_the_range_was)
{ for (std::uint32_t index = 0u; index <= most; index++) many_regions[index] = run_at(index, 1u);
  const TileRange ranges[1] = {{0u, 100u, most + 1u}};
  const ScriptedSource source(many_regions, most + 1u, ranges, 1u);
  TilePieces pieces(source);
  CHECK(pieces.next());
  CHECK_EQ(pieces.region_count(), most);
  CHECK_EQ(pieces.range_count(), most);
  CHECK(same(pieces.ranges()[most - 1u], 0u, 100u + most - 1u, 1u));
  CHECK(pieces.next());
  CHECK_EQ(pieces.region_count(), 1u);
  CHECK_EQ(pieces.regions()[0].start.x, most);
  CHECK(same(pieces.ranges()[0], 0u, 100u + most, 1u));
  CHECK(!pieces.next()); }

TEST(tile_pieces, a_region_whose_ranges_overflow_this_piece_opens_the_next)
{ for (std::uint32_t index = 0u; index <= most; index++) many_ranges[index] = TileRange{0u, index, 1u};
  const TileRegion regions[2] = {run_at(0u, 1u), run_at(1u, most)};
  const ScriptedSource source(regions, 2u, many_ranges, most + 1u);
  TilePieces pieces(source);
  CHECK(pieces.next());
  CHECK_EQ(pieces.region_count(), 1u);
  CHECK_EQ(pieces.range_count(), 1u);
  CHECK(pieces.next());
  CHECK_EQ(pieces.region_count(), 1u);
  CHECK_EQ(pieces.range_count(), most);
  CHECK(same(pieces.ranges()[0], 0u, 1u, 1u));
  CHECK(!pieces.next());
  CHECK(!pieces.cut_short()); }

TEST(tile_pieces, a_region_no_piece_holds_the_ranges_of_stops_the_update_there)
{ for (std::uint32_t index = 0u; index <= most; index++) many_ranges[index] = TileRange{0u, index, 1u};
  const TileRegion regions[2] = {run_at(0u, most + 1u), run_at(9u, 1u)};
  const ScriptedSource source(regions, 2u, many_ranges, most + 1u);
  TilePieces pieces(source);
  CHECK(!pieces.next());
  CHECK(pieces.cut_short());
  CHECK_EQ(pieces.region_count(), 0u);
  CHECK(!pieces.next()); }

TEST(tile_pieces, the_update_ends_where_its_regions_or_its_ranges_run_out)
{ /* Ranges out mid-region: the region goes, mapped as far as they reach. */
  const TileRegion five[1] = {run_at(0u, 5u)};
  const TileRange three[1] = {{0u, 0u, 3u}};
  const ScriptedSource short_ranges(five, 1u, three, 1u);
  TilePieces cut(short_ranges);
  CHECK(cut.next());
  CHECK_EQ(cut.regions()[0].size.tiles, 5u);
  CHECK_EQ(cut.range_count(), 1u);
  CHECK(same(cut.ranges()[0], 0u, 0u, 3u));
  CHECK(!cut.next());
  CHECK(!cut.cut_short());
  /* Regions out: the ranges left map nothing. */
  const TileRegion one[1] = {run_at(0u, 1u)};
  const TileRange two[2] = {{0u, 0u, 1u}, {0u, 5u, 1u}};
  const ScriptedSource short_regions(one, 1u, two, 2u);
  TilePieces done(short_regions);
  CHECK(done.next());
  CHECK_EQ(done.range_count(), 1u);
  CHECK(!done.next());
  /* None of either is no piece, and nothing cut short. */
  const ScriptedSource nothing(one, 0u, two, 2u);
  TilePieces empty(nothing);
  CHECK(!empty.next());
  CHECK(!empty.cut_short());
  const ScriptedSource no_ranges(one, 1u, two, 0u);
  TilePieces unranged(no_ranges);
  CHECK(!unranged.next());
  CHECK(!unranged.cut_short()); }

TEST(tile_pieces, a_range_of_no_tiles_ends_at_once_and_one_of_all_takes_every_tile_left)
{ const TileRegion regions[2] = {run_at(0u, 2u), run_at(4u, 3u)};
  const TileRange ranges[2] = {{0u, 6u, 0u}, {0u, 0u, as_many_as_are_left}};
  const ScriptedSource source(regions, 2u, ranges, 2u);
  TilePieces pieces(source);
  CHECK(pieces.next());
  CHECK_EQ(pieces.range_count(), 2u);
  CHECK(same(pieces.ranges()[0], 0u, 0u, 2u));
  CHECK(same(pieces.ranges()[1], 0u, 2u, 3u));
  /* A region of no tiles takes no range. */
  const TileRegion none[1] = {run_at(0u, 0u)};
  const ScriptedSource empty_region(none, 1u, ranges, 2u);
  TilePieces taken(empty_region);
  CHECK(taken.next());
  CHECK_EQ(taken.region_count(), 1u);
  CHECK_EQ(taken.range_count(), 0u); }
