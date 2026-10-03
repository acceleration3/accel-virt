/* Which regions of a reserved resource's tiles a guest may name: a run of
 * tiles counted on from its start, or a box of exactly its count, every tile
 * of either inside the resource the way vkd3d-proton numbers them - and a
 * packed mip's tiles counted from where the packed tiles start. The resource
 * here has 10 tiles: a 4 x 2 subresource at tile 0, its packed mips at 8. */
#include "executor/d3d/d3d12_tile_space.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3d12TileSpace;
using av::d3d12proto::TileCoordinate;
using av::d3d12proto::TileSize;

namespace {

constexpr std::uint32_t tiles = 10u;
constexpr std::uint32_t packed_start = 8u;
constexpr D3D12_SUBRESOURCE_TILING top = {4u, 2u, 1u, 0u};
constexpr D3D12_SUBRESOURCE_TILING packed = {1u, 1u, 1u, 0xFFFFFFFFu};

TileSize run_of(std::uint32_t count)
{
    return TileSize{count, 0u, 0u, 0u, 0u};
}

TileSize box_of(std::uint32_t width, std::uint32_t height, std::uint32_t depth)
{
    return TileSize{width * height * depth, 1u, width, height, depth};
}

}  // namespace

TEST(d3d12_tile_space, a_run_holds_up_to_the_last_tile_and_not_one_past)
{ const D3d12TileSpace space(tiles, packed_start);
  /* (1, 0) is tile 1: nine tiles from it end on tile 9, ten would pass it. */
  CHECK(space.holds(TileCoordinate{1u, 0u, 0u, 0u}, run_of(9u), top));
  CHECK(!space.holds(TileCoordinate{1u, 0u, 0u, 0u}, run_of(10u), top));
  /* (2, 1) is tile 2 + 4 * 1. */
  CHECK(space.holds(TileCoordinate{2u, 1u, 0u, 0u}, run_of(4u), top));
  CHECK(!space.holds(TileCoordinate{2u, 1u, 0u, 0u}, run_of(5u), top));
  /* A run of none, or of more than the resource has, is no region. */
  CHECK(!space.holds(TileCoordinate{}, run_of(0u), top));
  CHECK(!space.holds(TileCoordinate{}, run_of(tiles + 1u), top)); }

TEST(d3d12_tile_space, a_box_holds_when_its_far_corner_is_a_tile_and_it_is_its_count)
{ const D3d12TileSpace space(tiles, packed_start);
  CHECK(space.holds(TileCoordinate{}, box_of(4u, 2u, 1u), top));
  /* Its far corner (4, 2) is tile 12, past the resource. */
  CHECK(!space.holds(TileCoordinate{1u, 1u, 0u, 0u}, box_of(4u, 2u, 1u), top));
  /* (3, 1) is tile 7: the last tile of the subresource, still a tile. */
  CHECK(space.holds(TileCoordinate{2u, 0u, 0u, 0u}, box_of(2u, 2u, 1u), top));
  /* A box whose count is not its tiles, or with a side of none, which
   * vkd3d-proton walks by dividing by its width. */
  TileSize miscounted = box_of(4u, 2u, 1u);
  miscounted.tiles = 7u;
  CHECK(!space.holds(TileCoordinate{}, miscounted, top));
  CHECK(!space.holds(TileCoordinate{}, TileSize{1u, 1u, 0u, 1u, 1u}, top));
  CHECK(!space.holds(TileCoordinate{}, TileSize{1u, 1u, 1u, 0u, 1u}, top));
  CHECK(!space.holds(TileCoordinate{}, TileSize{1u, 1u, 1u, 1u, 0u}, top)); }

TEST(d3d12_tile_space, a_packed_mip_counts_from_where_the_packed_tiles_start)
{ const D3d12TileSpace space(tiles, packed_start);
  /* Tiles 8 and 9, whatever its y and z say. */
  CHECK(space.holds(TileCoordinate{0u, 0u, 0u, 1u}, run_of(2u), packed));
  CHECK(space.holds(TileCoordinate{1u, 5u, 5u, 1u}, run_of(1u), packed));
  CHECK(!space.holds(TileCoordinate{1u, 0u, 0u, 1u}, run_of(2u), packed));
  CHECK(space.holds(TileCoordinate{0u, 0u, 0u, 1u}, box_of(2u, 1u, 1u), packed));
  CHECK(!space.holds(TileCoordinate{1u, 0u, 0u, 1u}, box_of(2u, 1u, 1u), packed)); }

TEST(d3d12_tile_space, coordinates_whose_index_passes_2_to_the_64_are_no_tile)
{ const D3d12TileSpace space(tiles, packed_start);
  /* A host tiling that wide, at rows that far down, numbers its tiles past
   * 2^64: wrapped, the index would land inside the resource. */
  const D3D12_SUBRESOURCE_TILING wide = {0xFFFFFFFFu, 1u, 1u, 0u};
  CHECK(!space.holds(TileCoordinate{0u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0u}, run_of(1u), wide));
  /* Exactly 2^64 - 1 tiles across, and one more from x. */
  CHECK(!space.holds(TileCoordinate{1u, 0xFFFFFFFFu, 2u, 0u}, run_of(1u), wide));
  /* And a start just short of 2^64 is not brought back by the count. */
  CHECK(!space.holds(TileCoordinate{0u, 0xFFFFFFFFu, 2u, 0u}, run_of(2u), wide));
  /* A box whose far corner is that far down, though it starts inside. */
  CHECK(!space.holds(TileCoordinate{0u, 0xFFFFFFFFu, 0u, 0u}, box_of(1u, 1u, 4u), wide)); }
