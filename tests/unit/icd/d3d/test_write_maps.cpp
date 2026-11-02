#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/d3d/write_maps.hpp"

#include <cstdint>

using av::MockPlatform;
using av::Status;
using av::d3d::WriteMaps;

namespace av::d3d {

struct WriteMapsProbe {
    static Status open_unnamed(WriteMaps &maps, std::uint32_t subresource)
    {
        std::uint32_t slot = 0u;
        return maps.flags_.insert(subresource, 0u, slot);
    }
};

}

using av::d3d::WriteMapsProbe;

namespace {

constexpr std::uint32_t discard = 0x2u;
constexpr std::uint32_t no_overwrite = 0x1u;

}

TEST(write_maps, any_subresource_is_closed_with_its_own_maps_flags)
{ MockPlatform platform;
  WriteMaps maps(platform);
  CHECK_STATUS(maps.open(7u, 59u, discard), Status::ok);
  CHECK_STATUS(maps.open(7u, 3u, no_overwrite), Status::ok);
  CHECK(maps.invariants_hold());
  std::uint32_t flags = 9u;
  CHECK(maps.close(7u, 59u, flags));
  CHECK_EQ(flags, discard);
  CHECK(maps.close(7u, 3u, flags));
  CHECK_EQ(flags, no_overwrite);
  CHECK_EQ(maps.count(), 0u);
  CHECK(maps.invariants_hold()); }

TEST(write_maps, a_map_never_opened_closes_nothing_and_one_closes_once)
{ MockPlatform  platform;
  WriteMaps     maps(platform);
  std::uint32_t flags = 9u;
  CHECK(!maps.close(7u, 0u, flags));
  CHECK_EQ(flags, 0u);
  CHECK_STATUS(maps.open(7u, 0u, 0u), Status::ok);
  CHECK(maps.close(7u, 0u, flags));
  CHECK(!maps.close(7u, 0u, flags));
  CHECK_STATUS(maps.open(0u, 0u, discard), Status::invalid_parameter);
  CHECK_EQ(maps.count(), 0u); }

TEST(write_maps, a_second_map_of_one_subresource_takes_the_newer_flags)
{ MockPlatform platform;
  WriteMaps maps(platform);
  CHECK_STATUS(maps.open(4u, 1u, discard), Status::ok);
  CHECK_STATUS(maps.open(4u, 1u, no_overwrite), Status::ok);
  CHECK_EQ(maps.count(), 1u);
  std::uint32_t flags = 0u;
  CHECK(maps.close(4u, 1u, flags));
  CHECK_EQ(flags, no_overwrite); }

TEST(write_maps, a_destroyed_resource_takes_its_maps_and_only_its)
{ MockPlatform platform;
  WriteMaps maps(platform);
  CHECK_STATUS(maps.open(4u, 0u, 0u), Status::ok);
  CHECK_STATUS(maps.open(5u, 0u, discard), Status::ok);
  CHECK_STATUS(maps.open(4u, 2u, 0u), Status::ok);
  CHECK_STATUS(maps.open(4u, 9u, 0u), Status::ok);
  maps.forget(4u);
  CHECK_EQ(maps.count(), 1u);
  CHECK(maps.invariants_hold());
  std::uint32_t flags = 0u;
  CHECK(maps.close(5u, 0u, flags));
  CHECK_EQ(flags, discard); }

TEST(write_maps, holds_every_map_an_application_keeps_open)
{ MockPlatform platform;
  WriteMaps    maps(platform);
  for (std::uint32_t resource = 1u; resource <= 5000u; resource++)
      CHECK_STATUS(maps.open(resource, resource % 7u, resource % 2u == 0u ? discard : no_overwrite), Status::ok);
  CHECK_EQ(maps.count(), 5000u);
  CHECK(maps.invariants_hold());
  for (std::uint32_t resource = 1u; resource <= 5000u; resource++) {
      std::uint32_t flags = 0u;
      CHECK(maps.close(resource, resource % 7u, flags));
      CHECK_EQ(flags, resource % 2u == 0u ? discard : no_overwrite);
  }
  CHECK_EQ(maps.count(), 0u);
  CHECK(maps.invariants_hold()); }

TEST(write_maps, a_map_with_no_memory_for_it_is_refused_and_the_others_kept)
{ MockPlatform platform;
  WriteMaps    maps(platform);
  for (std::uint32_t index = 0u; index < 8u; index++) CHECK_STATUS(maps.open(1u, index, 0u), Status::ok);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(maps.open(2u, 0u, discard), Status::no_memory);
  CHECK_STATUS(maps.open(1u, 5u, discard), Status::ok);
  CHECK_EQ(maps.count(), 8u);
  std::uint32_t flags = 0u;
  CHECK(!maps.close(2u, 0u, flags));
  CHECK(maps.close(1u, 5u, flags));
  CHECK_EQ(flags, discard);
  CHECK_STATUS(maps.open(2u, 0u, discard), Status::ok);
  CHECK(maps.invariants_hold()); }

TEST(write_maps, the_invariant_catches_a_map_of_no_resource)
{ MockPlatform platform;
  WriteMaps    maps(platform);
  CHECK_STATUS(maps.open(3u, 0u, 0u), Status::ok);
  CHECK(maps.invariants_hold());
  CHECK_STATUS(WriteMapsProbe::open_unnamed(maps, 4u), Status::ok);
  CHECK(!maps.invariants_hold()); }
