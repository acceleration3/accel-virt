#include "harness/framework/test_registry.hpp"
#include "icd/d3d/list_pieces.hpp"

#include <cstdint>

using av::icd::ListPieces;

TEST(list_pieces, a_long_list_goes_in_pieces_of_the_limit)
{ ListPieces pieces(40u, 16u);
  const std::uint64_t offsets[] = {0u, 16u, 32u};
  const std::uint64_t counts[] = {16u, 16u, 8u};
  for (std::uint32_t index = 0u; index < 3u; index++) {
      CHECK(pieces.next());
      CHECK_EQ(pieces.offset(), offsets[index]);
      CHECK_EQ(pieces.count(), counts[index]);
  }
  CHECK(!pieces.next()); }

TEST(list_pieces, a_list_the_limit_divides_has_no_empty_last_piece)
{ ListPieces pieces(32u, 16u);
  CHECK(pieces.next());
  CHECK(pieces.next());
  CHECK_EQ(pieces.offset(), 16u);
  CHECK(!pieces.next()); }

TEST(list_pieces, an_empty_list_is_one_piece_of_none)
{ ListPieces empty(0u, 16u);
  CHECK(empty.next());
  CHECK_EQ(empty.offset(), 0u);
  CHECK_EQ(empty.count(), 0u);
  CHECK(!empty.next());
  ListPieces short_list(3u, 16u);
  CHECK(short_list.next());
  CHECK_EQ(short_list.count(), 3u);
  CHECK(!short_list.next()); }

TEST(list_pieces, a_limit_of_nothing_sends_nothing)
{ ListPieces stuck(4u, 0u);
  CHECK(!stuck.next());
  ListPieces none(0u, 0u);
  CHECK(!none.next()); }
