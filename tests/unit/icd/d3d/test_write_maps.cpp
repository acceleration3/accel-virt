/* The subresources a device has open for writing (WriteMaps): each with its
 * own map's flags, any subresource of any resource, closed once, forgotten
 * with its resource, and a full table refusing the next map rather than
 * losing a write at the unmap. Invariants after every change. */
#include "harness/framework/test_registry.hpp"
#include "icd/d3d/write_maps.hpp"

#include <cstdint>

using av::d3d::WriteMaps;

namespace av::d3d {

struct WriteMapsProbe {
    static void set_count(WriteMaps &maps, std::uint32_t count) { maps.count_ = count; }
    static void set_flags(WriteMaps &maps, std::uint32_t row, std::uint32_t flags) { maps.flags_[row] = flags; }
    static void set_row(WriteMaps &maps, std::uint32_t row, std::uint32_t resource, std::uint32_t subresource)
    {
        maps.resources_[row] = resource;
        maps.subresources_[row] = subresource;
    }
};

}  // namespace av::d3d

using av::d3d::WriteMapsProbe;

namespace {

constexpr std::uint32_t discard = 0x2u;
constexpr std::uint32_t no_overwrite = 0x1u;

}  // namespace

/* A cube's sixth face of a ten-mip texture is subresource 59: past any
 * 32-bit mask, and still carried back at its unmap. */
TEST(write_maps, any_subresource_is_closed_with_its_own_maps_flags)
{ WriteMaps maps;
  CHECK(maps.open(7u, 59u, discard));
  CHECK(maps.open(7u, 3u, no_overwrite));
  CHECK(maps.invariants_hold());
  std::uint32_t flags = 9u;
  CHECK(maps.close(7u, 59u, flags));
  CHECK_EQ(flags, discard);
  CHECK(maps.close(7u, 3u, flags));
  CHECK_EQ(flags, no_overwrite);
  CHECK_EQ(maps.count(), 0u);
  CHECK(maps.invariants_hold()); }

/* A read or a subresource never mapped closes nothing, and a map closes
 * once. */
TEST(write_maps, a_map_never_opened_closes_nothing_and_one_closes_once)
{ WriteMaps     maps;
  std::uint32_t flags = 9u;
  CHECK(!maps.close(7u, 0u, flags));
  CHECK_EQ(flags, 0u);
  CHECK(maps.open(7u, 0u, 0u));
  CHECK(maps.close(7u, 0u, flags));
  CHECK(!maps.close(7u, 0u, flags));
  CHECK(!maps.open(0u, 0u, discard));
  CHECK_EQ(maps.count(), 0u); }

/* Mapped again before its unmap, a subresource keeps one row with the newer
 * map's flags. */
TEST(write_maps, a_second_map_of_one_subresource_takes_the_newer_flags)
{ WriteMaps maps;
  CHECK(maps.open(4u, 1u, discard));
  CHECK(maps.open(4u, 1u, no_overwrite));
  CHECK_EQ(maps.count(), 1u);
  std::uint32_t flags = 0u;
  CHECK(maps.close(4u, 1u, flags));
  CHECK_EQ(flags, no_overwrite); }

TEST(write_maps, a_destroyed_resource_takes_its_maps_and_only_its)
{ WriteMaps maps;
  CHECK(maps.open(4u, 0u, 0u));
  CHECK(maps.open(5u, 0u, discard));
  CHECK(maps.open(4u, 2u, 0u));
  CHECK(maps.open(4u, 9u, 0u));
  maps.forget(4u);
  CHECK_EQ(maps.count(), 1u);
  CHECK(maps.invariants_hold());
  std::uint32_t flags = 0u;
  CHECK(maps.close(5u, 0u, flags));
  CHECK_EQ(flags, discard); }

TEST(write_maps, a_full_table_refuses_the_next_map_and_takes_one_after_a_close)
{ WriteMaps maps;
  for (std::uint32_t index = 0u; index < WriteMaps::capacity; index++) CHECK(maps.open(1u, index, 0u));
  CHECK(!maps.open(2u, 0u, discard));
  /* One already open is still taken: it needs no new row. */
  CHECK(maps.open(1u, 5u, discard));
  std::uint32_t flags = 0u;
  CHECK(maps.close(1u, 0u, flags));
  CHECK(maps.open(2u, 0u, discard));
  CHECK(maps.invariants_hold()); }

TEST(write_maps, the_invariant_catches_every_state_the_table_never_builds)
{ for (int knob = 0; knob < 6; knob++) {
      WriteMaps maps;
      CHECK(maps.open(3u, 0u, 0u));
      CHECK(maps.open(3u, 1u, 0u));
      if (knob == 0) WriteMapsProbe::set_row(maps, 1u, 3u, 0u);       /* one subresource twice */
      if (knob == 1) WriteMapsProbe::set_row(maps, 0u, 0u, 0u);       /* an open row of nothing */
      if (knob == 2) WriteMapsProbe::set_row(maps, 5u, 3u, 7u);       /* a row past the count */
      if (knob == 3) WriteMapsProbe::set_count(maps, WriteMaps::capacity + 1u);
      if (knob == 4) WriteMapsProbe::set_row(maps, 5u, 0u, 7u);       /* a subresource past the count */
      if (knob == 5) WriteMapsProbe::set_flags(maps, 5u, discard);    /* flags past the count */
      CHECK(!maps.invariants_hold());
  } }
