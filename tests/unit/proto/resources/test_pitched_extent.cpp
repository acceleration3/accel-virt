#include "harness/framework/test_registry.hpp"
#include "proto/resources/pitched_extent.hpp"

#include <cstdint>

using av::PitchedExtent;

TEST(pitched_extent, an_extent_with_no_line_row_or_slice_is_empty)
{ CHECK(PitchedExtent(0u, 1u, 1u, 16u, 16u).empty());
  CHECK(PitchedExtent(4u, 0u, 1u, 16u, 16u).empty());
  CHECK(PitchedExtent(4u, 1u, 0u, 16u, 16u).empty());
  CHECK(!PitchedExtent(4u, 1u, 1u, 16u, 16u).empty()); }

TEST(pitched_extent_pitches_hold, a_pitch_must_hold_the_line_and_a_slice_pitch_the_rows)
{ CHECK(PitchedExtent(16u, 4u, 1u, 16u, 0u).pitches_hold());
  CHECK(!PitchedExtent(16u, 4u, 1u, 15u, 0u).pitches_hold());
  CHECK(PitchedExtent(16u, 1u, 1u, 0u, 0u).pitches_hold());
  CHECK(PitchedExtent(16u, 4u, 2u, 16u, 64u).pitches_hold());
  CHECK(PitchedExtent(16u, 4u, 2u, 16u, 64u - 16u + 16u).pitches_hold());
  CHECK(!PitchedExtent(16u, 4u, 2u, 16u, 63u).pitches_hold());
  CHECK(!PitchedExtent(0u, 4u, 1u, 16u, 64u).pitches_hold());
  CHECK(!PitchedExtent(16u, ~std::uint64_t{0}, 2u, 16u, ~std::uint64_t{0}).pitches_hold());
  CHECK(!PitchedExtent(16u, 2u, 2u, ~std::uint64_t{0}, ~std::uint64_t{0}).pitches_hold()); }

TEST(pitched_extent, ends_at_the_last_line_of_the_last_slice_and_not_at_the_pitch)
{ const PitchedExtent extent(10u, 3u, 2u, 16u, 64u);
  std::uint64_t offset = 0u;
  CHECK(extent.offset_of(2u, 1u, offset));
  CHECK_EQ(offset, 64u + 32u);
  std::uint64_t end = 0u;
  CHECK(extent.end(100u, end));
  CHECK_EQ(end, 100u + 64u + 32u + 10u);
  CHECK(extent.within(100u + 64u + 32u + 10u - 100u));
  CHECK(!extent.within(64u + 32u + 9u));
  CHECK(extent.fits(106u));
  CHECK(!extent.fits(105u));
  CHECK(!PitchedExtent(10u, 3u, 2u, 8u, 64u).fits(1000u));
  CHECK(!PitchedExtent(0u, 3u, 2u, 16u, 64u).within(1000u)); }

TEST(pitched_extent, refuses_an_offset_or_end_that_would_wrap)
{ const PitchedExtent huge(8u, ~std::uint64_t{0}, 1u, ~std::uint64_t{0}, 0u);
  std::uint64_t offset = 0u;
  CHECK(!huge.offset_of(~std::uint64_t{0} - 1u, 0u, offset));
  std::uint64_t end = 0u;
  CHECK(!huge.end(0u, end));
  const PitchedExtent plain(8u, 1u, 1u, 8u, 8u);
  CHECK(!plain.end(~std::uint64_t{0}, end)); }
