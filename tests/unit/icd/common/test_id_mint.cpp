#include "harness/framework/test_registry.hpp"
#include "icd/common/id_mint.hpp"

#include <cstdint>

using av::icd::IdMint;

TEST(id_mint, hands_out_ids_from_one_and_knows_the_ones_it_issued)
{ IdMint<std::uint32_t> mint;
  CHECK(!mint.issued(0u));
  CHECK(!mint.issued(1u));
  CHECK_EQ(mint.next(), 1u);
  CHECK_EQ(mint.next(), 2u);
  CHECK(mint.issued(1u));
  CHECK(mint.issued(2u));
  CHECK(!mint.issued(3u));
  CHECK(!mint.issued(0u)); }

TEST(id_mint, a_run_names_its_first_id_and_takes_the_whole_range)
{ IdMint<std::uint16_t> mint;
  CHECK_EQ(mint.run(5u), 1u);
  CHECK(mint.issued(5u));
  CHECK(!mint.issued(6u));
  CHECK_EQ(mint.run(0u), 6u);
  CHECK_EQ(mint.next(), 7u); }

TEST(id_mint, wraps_to_one_rather_than_issuing_zero_or_a_range_past_the_end)
{ IdMint<std::uint8_t> mint;
  CHECK_EQ(mint.run(250u), 1u);
  CHECK_EQ(mint.run(5u), 251u);
  CHECK_EQ(mint.run(1u), 1u);
  CHECK_EQ(mint.run(200u), 2u);
  CHECK_EQ(mint.run(100u), 1u); }
