#include "executor/d3d/d3d12_row_copies.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3d12RowCopies;

namespace {

constexpr std::uint32_t rgb32 = 6u;
constexpr std::uint32_t rgba32 = 2u;
constexpr std::uint32_t bc1 = 71u;

}

TEST(d3d12_row_copies, only_a_texel_that_does_not_divide_the_pitch_needs_rows)
{ CHECK(D3d12RowCopies(rgb32, 4u, 256u).needed());
  CHECK(!D3d12RowCopies(rgb32, 4u, 192u).needed());
  CHECK(!D3d12RowCopies(rgba32, 4u, 256u).needed());
  CHECK(!D3d12RowCopies(bc1, 4u, 256u).needed());
  CHECK(!D3d12RowCopies(0xFFFFu, 4u, 256u).needed()); }

TEST(d3d12_row_copies, a_row_sits_a_pitch_after_the_one_before_and_a_slice_a_footprint_after_that)
{ const D3d12RowCopies rows(rgb32, 3u, 256u);
  CHECK_EQ(rows.row_offset(512u, 0u, 0u, 0u), 512u);
  CHECK_EQ(rows.row_offset(512u, 0u, 2u, 0u), 512u + 512u);
  CHECK_EQ(rows.row_offset(512u, 2u, 1u, 0u), 512u + 2u * 768u + 256u);
  CHECK_EQ(rows.row_offset(0u, 0u, 0u, 3u), 36u); }

TEST(d3d12_row_copies, a_single_row_is_packed_to_whole_texels)
{ const D3d12RowCopies rows(rgb32, 3u, 256u);
  CHECK_EQ(rows.pitch_of(5u), 60u);
  CHECK_EQ(rows.pitch_of(1u), 12u); }
