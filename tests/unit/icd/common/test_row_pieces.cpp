#include "harness/framework/test_registry.hpp"
#include "icd/common/row_pieces.hpp"
#include "proto/resources/texel_layout.hpp"

#include <cstdint>

using av::TexelLayout;
using av::icd::RowPieces;

namespace {
const TexelLayout blocks(8u, 4u, 4u);
const TexelLayout words(4u, 1u, 1u);

}

TEST(row_pieces, block_rows_are_cut_by_the_limit_and_clamped_to_the_region)
{ RowPieces pieces(blocks, 16u, 18u, 64u, 150u);
  CHECK(pieces.next());
  CHECK(pieces.first());
  CHECK_EQ(pieces.top(), 0u);
  CHECK_EQ(pieces.bottom(), 8u);
  CHECK_EQ(pieces.left(), 0u);
  CHECK_EQ(pieces.right(), 16u);
  CHECK_EQ(pieces.offset(), 0u);
  CHECK_EQ(pieces.bytes(), 96u);
  CHECK(pieces.next());
  CHECK(!pieces.first());
  CHECK_EQ(pieces.top(), 8u);
  CHECK_EQ(pieces.bottom(), 16u);
  CHECK_EQ(pieces.offset(), 128u);
  CHECK(pieces.next());
  CHECK_EQ(pieces.top(), 16u);
  CHECK_EQ(pieces.bottom(), 18u);
  CHECK_EQ(pieces.bytes(), 32u);
  CHECK(!pieces.next()); }

TEST(row_pieces, a_piece_holds_exactly_a_line_and_the_pitches_before_it)
{ RowPieces two(blocks, 16u, 16u, 64u, 96u);
  CHECK(two.next());
  CHECK_EQ(two.bottom(), 8u);
  CHECK_EQ(two.bytes(), 96u);
  RowPieces one(blocks, 16u, 16u, 64u, 95u);
  CHECK(one.next());
  CHECK_EQ(one.bottom(), 4u);
  CHECK_EQ(one.bytes(), 32u); }

TEST(row_pieces, a_row_wider_than_the_limit_goes_in_runs_of_whole_elements)
{ RowPieces pieces(words, 10u, 2u, 48u, 18u);
  const std::uint32_t lefts[] = {0u, 4u, 8u, 0u, 4u, 8u};
  const std::uint32_t rights[] = {4u, 8u, 10u, 4u, 8u, 10u};
  const std::uint64_t offsets[] = {0u, 16u, 32u, 48u, 64u, 80u};
  for (std::uint32_t index = 0u; index < 6u; index++) {
      CHECK(pieces.next());
      CHECK_EQ(pieces.first(), index == 0u);
      CHECK_EQ(pieces.top(), index < 3u ? 0u : 1u);
      CHECK_EQ(pieces.bottom(), index < 3u ? 1u : 2u);
      CHECK_EQ(pieces.left(), lefts[index]);
      CHECK_EQ(pieces.right(), rights[index]);
      CHECK_EQ(pieces.offset(), offsets[index]);
      CHECK_EQ(pieces.bytes(), (rights[index] - lefts[index]) * 4u);
      CHECK(pieces.bytes() <= 18u);
  }
  CHECK(!pieces.next()); }

TEST(row_pieces, a_split_block_row_clamps_its_last_run_to_the_region)
{ RowPieces pieces(blocks, 10u, 4u, 32u, 16u);
  CHECK(pieces.next());
  CHECK_EQ(pieces.left(), 0u);
  CHECK_EQ(pieces.right(), 8u);
  CHECK(pieces.next());
  CHECK_EQ(pieces.left(), 8u);
  CHECK_EQ(pieces.right(), 10u);
  CHECK_EQ(pieces.bytes(), 8u);
  CHECK(!pieces.next()); }

TEST(row_pieces, nothing_to_send_is_no_piece)
{ RowPieces empty(words, 0u, 4u, 16u, 64u);
  CHECK(!empty.next());
  CHECK_EQ(empty.bytes(), 0u);
  RowPieces flat_region(words, 4u, 0u, 16u, 64u);
  CHECK(!flat_region.next());
  RowPieces unknown(TexelLayout(0u, 1u, 1u), 4u, 4u, 16u, 64u);
  CHECK(!unknown.next());
  RowPieces too_small(blocks, 4u, 4u, 8u, 7u);
  CHECK(!too_small.next());
  RowPieces flat(words, 4u, 3u, 0u, 64u);
  CHECK(flat.next());
  CHECK_EQ(flat.bottom(), 3u);
  CHECK(!flat.next()); }
