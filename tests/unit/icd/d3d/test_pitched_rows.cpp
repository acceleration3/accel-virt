#include "harness/framework/test_registry.hpp"
#include "icd/d3d/pitched_rows.hpp"

#include <cstdint>
#include <cstring>

using av::d3d::PitchedRows;

TEST(pitched_rows, each_row_takes_the_narrower_pitch)
{ std::uint8_t from_bytes[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  std::uint8_t into_bytes[12] = {};
  PitchedRows  from(from_bytes, 4u, sizeof from_bytes);
  PitchedRows  into(into_bytes, 6u, sizeof into_bytes);
  CHECK_EQ(into.copy_from(from, 3u), 2u);
  const std::uint8_t wanted[12] = {1, 2, 3, 4, 0, 0, 5, 6, 7, 8, 0, 0};
  CHECK(std::memcmp(into_bytes, wanted, sizeof wanted) == 0); }

TEST(pitched_rows, stops_where_the_source_or_the_count_ends)
{ std::uint8_t from_bytes[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  std::uint8_t into_bytes[16] = {};
  PitchedRows  from(from_bytes, 4u, sizeof from_bytes);
  PitchedRows  into(into_bytes, 4u, sizeof into_bytes);
  CHECK_EQ(into.copy_from(from, 9u), 2u);
  CHECK_EQ(into_bytes[8], 0u);
  CHECK_EQ(into.copy_from(from, 1u), 1u);
  PitchedRows nothing(nullptr, 4u, 64u);
  CHECK_EQ(into.copy_from(nothing, 4u), 0u);
  PitchedRows flat(from_bytes, 0u, sizeof from_bytes);
  CHECK_EQ(into.copy_from(flat, 4u), 0u); }
