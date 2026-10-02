/* A buffer write cut into pieces a command can carry (BytePieces): each at
 * most the limit, the last what is left, and only the first may discard. */
#include "harness/framework/test_registry.hpp"
#include "icd/d3d/byte_pieces.hpp"

#include <cstdint>

using av::d3d::BytePieces;

TEST(byte_pieces, cuts_at_the_limit_and_leaves_the_rest_last)
{ BytePieces pieces(10u, 4u);
  CHECK(pieces.next());
  CHECK(pieces.first());
  CHECK_EQ(pieces.offset(), 0u);
  CHECK_EQ(pieces.bytes(), 4u);
  CHECK(pieces.next());
  CHECK(!pieces.first());
  CHECK_EQ(pieces.offset(), 4u);
  CHECK_EQ(pieces.bytes(), 4u);
  CHECK(pieces.next());
  CHECK_EQ(pieces.offset(), 8u);
  CHECK_EQ(pieces.bytes(), 2u);
  CHECK(!pieces.next()); }

TEST(byte_pieces, a_write_under_the_limit_is_one_piece_and_nothing_is_none)
{ BytePieces whole(3u, 16u);
  CHECK(whole.next());
  CHECK_EQ(whole.bytes(), 3u);
  CHECK(!whole.next());
  BytePieces nothing(0u, 16u);
  CHECK(!nothing.next());
  /* No limit is no piece rather than a piece of nothing forever. */
  BytePieces stuck(8u, 0u);
  CHECK(!stuck.next()); }
