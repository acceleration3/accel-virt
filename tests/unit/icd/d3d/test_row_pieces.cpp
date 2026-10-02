/* One slice of a texture write cut into pieces of whole element rows
 * (RowPieces): as many rows as the limit holds at the pitch, at least one;
 * the texel rows each covers, clamped to the region; and the last row of a
 * piece only its line. */
#include "harness/framework/test_registry.hpp"
#include "icd/d3d/row_pieces.hpp"

#include <cstdint>

using av::d3d::RowPieces;

/* Five rows of 4x4 blocks (18 texels tall), 32 bytes a line at a pitch of
 * 64, two rows to a 150-byte piece. */
TEST(row_pieces, block_rows_are_cut_by_the_limit_and_clamped_to_the_region)
{ RowPieces pieces(5u, 18u, 32u, 64u, 150u);
  CHECK(pieces.next());
  CHECK(pieces.first());
  CHECK_EQ(pieces.top(), 0u);
  CHECK_EQ(pieces.bottom(), 8u);
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

/* A pitch past the limit still carries one row a piece; no rows is no
 * piece, and a pitch of nothing is one row a piece rather than a division. */
TEST(row_pieces, every_piece_carries_a_row_and_nothing_is_none)
{ RowPieces wide(2u, 2u, 100u, 200u, 64u);
  CHECK(wide.next());
  CHECK_EQ(wide.bytes(), 100u);
  CHECK(wide.next());
  CHECK_EQ(wide.offset(), 200u);
  CHECK(!wide.next());
  RowPieces none(0u, 0u, 4u, 4u, 64u);
  CHECK(!none.next());
  CHECK_EQ(none.bytes(), 0u);
  RowPieces flat(2u, 2u, 4u, 0u, 64u);
  CHECK(flat.next());
  CHECK(flat.next());
  CHECK(!flat.next()); }
