#include "core/support/object_storage.hpp"
#include "executor/d3d/d3d12_tile_update.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3d12TileUpdate;
using av::d3d12proto::TileCoordinate;
using av::d3d12proto::TileRange;
using av::d3d12proto::TileRegion;
using av::d3d12proto::TileSize;

namespace {
av::ObjectStorage<D3d12TileUpdate> storage;

class FreshUpdate {
public:
    FreshUpdate() { CHECK_STATUS(storage.construct(), av::Status::ok); }
    ~FreshUpdate() { storage.destroy(); }
    FreshUpdate(const FreshUpdate &) = delete;
    FreshUpdate &operator=(const FreshUpdate &) = delete;

    D3d12TileUpdate &update() { return storage.object(); }
};

constexpr std::uint32_t range_null = 0x1u;
constexpr std::uint32_t range_skip = 0x2u;
constexpr std::uint32_t range_reuse = 0x4u;
constexpr TileRegion    one_tile = {TileCoordinate{}, TileSize{1u, 0u, 0u, 0u, 0u}};

}

TEST(d3d12_tile_update, a_range_maps_only_inside_its_heap)
{ FreshUpdate fresh;
  D3d12TileUpdate &update = fresh.update();
  CHECK(update.add_range(TileRange{0u, 2u, 2u}, 4u));
  CHECK(!update.add_range(TileRange{0u, 3u, 2u}, 4u));
  CHECK(update.add_range(TileRange{range_reuse, 3u, 100u}, 4u));
  CHECK(!update.add_range(TileRange{range_reuse, 4u, 1u}, 4u));
  CHECK(!update.add_range(TileRange{0u, 0u, 1u}, 0u));
  CHECK_EQ(update.ranges(), 2u); }

TEST(d3d12_tile_update, a_range_that_unmaps_or_skips_names_no_heap)
{ FreshUpdate fresh;
  D3d12TileUpdate &update = fresh.update();
  CHECK(update.add_range(TileRange{range_null, 0xFFFFFFFFu, 8u}, 0u));
  CHECK(update.add_range(TileRange{range_skip, 0xFFFFFFFFu, 8u}, 0u));
  CHECK_EQ(update.ranges(), 2u); }

TEST(d3d12_tile_update, a_range_of_no_tiles_or_of_no_flag_the_api_has_is_refused)
{ FreshUpdate fresh;
  D3d12TileUpdate &update = fresh.update();
  CHECK(!update.add_range(TileRange{range_null, 0u, 0u}, 4u));
  CHECK(!update.add_range(TileRange{0x8u, 0u, 1u}, 4u));
  CHECK(!update.add_range(TileRange{range_null | range_skip, 0u, 1u}, 4u));
  CHECK_EQ(update.ranges(), 0u); }

TEST(d3d12_tile_update, it_holds_one_command_s_worth_and_begins_again_empty)
{ FreshUpdate fresh;
  D3d12TileUpdate &update = fresh.update();
  for (std::uint32_t index = 0u; index < D3d12TileUpdate::most; index++) {
      CHECK(update.add_region(one_tile));
      CHECK(update.add_range(TileRange{range_null, 0u, 1u}, 0u));
  }
  CHECK(!update.add_region(one_tile));
  CHECK(!update.add_range(TileRange{range_null, 0u, 1u}, 0u));
  CHECK_EQ(update.regions(), D3d12TileUpdate::most);
  CHECK_EQ(update.ranges(), D3d12TileUpdate::most);
  update.begin();
  CHECK_EQ(update.regions(), 0u);
  CHECK_EQ(update.ranges(), 0u);
  CHECK(update.add_region(one_tile));
  CHECK(update.add_region(TileRegion{TileCoordinate{1u, 1u, 0u, 0u}, TileSize{4u, 1u, 2u, 2u, 1u}}));
  CHECK_EQ(update.regions(), 2u); }
