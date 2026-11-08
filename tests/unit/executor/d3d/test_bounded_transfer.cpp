#include "executor/d3d/bounded_transfer.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::BoundedTransfer;

namespace {
constexpr std::uint64_t mib = std::uint64_t{1} << 20u;
constexpr std::uint64_t bound = BoundedTransfer::most_bytes;
}

TEST(bounded_transfer, a_transfer_within_the_bound_is_one_piece_that_is_not_split)
{ BoundedTransfer transfer(bound, 1u);
  CHECK(!transfer.split());
  std::uint64_t first = 9u, count = 9u;
  CHECK(transfer.next(first, count));
  CHECK_EQ(first, 0u);
  CHECK_EQ(count, bound);
  CHECK(!transfer.next(first, count));
  CHECK(transfer.invariants_hold()); }

TEST(bounded_transfer, a_byte_past_the_bound_is_split_and_the_pieces_cover_the_units_exactly_once)
{ BoundedTransfer transfer(bound + 1u, 1u);
  CHECK(transfer.split());
  std::uint64_t first = 0u, count = 0u;
  CHECK(transfer.next(first, count));
  CHECK_EQ(first, 0u);
  CHECK_EQ(count, bound);
  CHECK(transfer.next(first, count));
  CHECK_EQ(first, bound);
  CHECK_EQ(count, 1u);
  CHECK(!transfer.next(first, count));
  CHECK(transfer.invariants_hold()); }

TEST(bounded_transfer, hundreds_of_mib_of_rows_become_pieces_none_over_the_bound)
{ constexpr std::uint64_t row = 4096u * 16u;
  constexpr std::uint64_t rows = 640u * mib / row;
  BoundedTransfer transfer(rows, row);
  std::uint64_t first = 0u, count = 0u, covered = 0u, pieces = 0u;
  while (transfer.next(first, count)) {
    CHECK_EQ(first, covered);
    CHECK(count * row <= bound);
    covered += count;
    pieces++;
  }
  CHECK_EQ(covered, rows);
  CHECK_EQ(pieces, 10u); }

TEST(bounded_transfer, a_unit_larger_than_the_bound_goes_alone_and_zero_sizes_do_not_loop)
{ BoundedTransfer big(3u, 2u * bound);
  std::uint64_t first = 0u, count = 0u;
  CHECK(big.split());
  CHECK(big.next(first, count));
  CHECK_EQ(count, 1u);
  BoundedTransfer empty(0u, 4u);
  CHECK(!empty.split());
  CHECK(!empty.next(first, count));
  BoundedTransfer free_units(5u, 0u);
  CHECK(!free_units.split());
  CHECK(free_units.next(first, count));
  CHECK_EQ(count, 5u); }
