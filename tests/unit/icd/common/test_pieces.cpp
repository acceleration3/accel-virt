#include "harness/framework/test_registry.hpp"
#include "icd/common/pieces.hpp"

#include <cstdint>

using av::icd::Pieces;

TEST(pieces, cuts_at_the_limit_and_leaves_the_rest_last)
{ Pieces pieces(10u, 4u);
  CHECK(pieces.next());
  CHECK(pieces.first());
  CHECK_EQ(pieces.offset(), 0u);
  CHECK_EQ(pieces.count(), 4u);
  CHECK(pieces.next());
  CHECK(!pieces.first());
  CHECK_EQ(pieces.offset(), 4u);
  CHECK_EQ(pieces.count(), 4u);
  CHECK(pieces.next());
  CHECK_EQ(pieces.offset(), 8u);
  CHECK_EQ(pieces.count(), 2u);
  CHECK(!pieces.next()); }

TEST(pieces, a_run_the_limit_divides_has_no_empty_last_piece)
{ Pieces pieces(8u, 4u);
  CHECK(pieces.next());
  CHECK(pieces.next());
  CHECK_EQ(pieces.offset(), 4u);
  CHECK_EQ(pieces.count(), 4u);
  CHECK(!pieces.next()); }

TEST(pieces, a_run_under_the_limit_is_one_piece_and_nothing_is_none)
{ Pieces whole(3u, 16u);
  CHECK(whole.next());
  CHECK_EQ(whole.count(), 3u);
  CHECK(!whole.next());
  Pieces nothing(0u, 16u);
  CHECK(!nothing.next());
  Pieces stuck(8u, 0u);
  CHECK(!stuck.next()); }
