#include "harness/framework/test_registry.hpp"
#include "proto/resources/mip_extent.hpp"

#include <cstdint>

using av::MipExtent;

TEST(mip_extent, halves_each_edge_per_level_and_never_below_one)
{ const MipExtent base(64u, 32u, 16u, 0u);
  CHECK_EQ(base.width(), 64u);
  CHECK_EQ(base.height(), 32u);
  CHECK_EQ(base.depth(), 16u);
  const MipExtent third(64u, 32u, 16u, 3u);
  CHECK_EQ(third.width(), 8u);
  CHECK_EQ(third.height(), 4u);
  CHECK_EQ(third.depth(), 2u);
  const MipExtent last(64u, 32u, 16u, 6u);
  CHECK_EQ(last.width(), 1u);
  CHECK_EQ(last.height(), 1u);
  CHECK_EQ(last.depth(), 1u); }

TEST(mip_extent, a_level_past_the_width_of_a_word_is_one_texel)
{ const MipExtent far(std::uint64_t{1} << 40u, 1u << 20u, 1u << 10u, 64u);
  CHECK_EQ(far.width(), 1u);
  CHECK_EQ(far.height(), 1u);
  CHECK_EQ(far.depth(), 1u);
  const MipExtent wide(std::uint64_t{1} << 40u, 8u, 8u, 20u);
  CHECK_EQ(wide.width(), std::uint64_t{1} << 20u);
  const MipExtent empty(0u, 0u, 0u, 0u);
  CHECK_EQ(empty.width(), 1u);
  CHECK_EQ(empty.height(), 1u); }
