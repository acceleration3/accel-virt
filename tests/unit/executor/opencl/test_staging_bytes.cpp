/* Private memory kept between transfers (StagingBytes): grown to whole pages
 * for a larger ask, kept for a smaller one, and a size that cannot be had
 * refused with what was held kept. */
#include "executor/opencl/staging_bytes.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::StagingBytes;

TEST(staging_bytes, an_ask_is_rounded_to_pages_and_a_smaller_one_keeps_the_memory)
{ StagingBytes staging;
  CHECK(staging.bytes() == nullptr);
  CHECK_EQ(staging.held(), 0u);
  /* Nothing asked is nothing mapped. */
  CHECK(staging.reserve(0u));
  CHECK(staging.bytes() == nullptr);
  CHECK(staging.reserve(1u));
  CHECK_EQ(staging.held(), 4096u);
  std::uint8_t *const first = staging.bytes();
  CHECK(first != nullptr);
  first[4095] = 0x5Au;
  /* A page exactly, and less: the same memory, its bytes as they were. */
  CHECK(staging.reserve(4096u));
  CHECK(staging.bytes() == first);
  CHECK_EQ(staging.bytes()[4095], 0x5Au);
  /* One past it is two pages, somewhere else. */
  CHECK(staging.reserve(4097u));
  CHECK_EQ(staging.held(), 8192u);
  staging.bytes()[8191] = 1u;
  CHECK_EQ(staging.bytes()[8191], 1u); }

TEST(staging_bytes, a_size_that_cannot_be_had_is_refused_and_what_was_held_stays)
{ StagingBytes staging;
  CHECK(staging.reserve(100u));
  std::uint8_t *const held = staging.bytes();
  /* Rounding it to a page would wrap. */
  CHECK(!staging.reserve(SIZE_MAX));
  CHECK(!staging.reserve(SIZE_MAX - 4095u));
  CHECK(staging.bytes() == held);
  CHECK_EQ(staging.held(), 4096u);
  /* Rounds without wrapping, and no machine maps it. */
  CHECK(!staging.reserve(SIZE_MAX - 8192u));
  CHECK(staging.bytes() == held);
  CHECK_EQ(staging.held(), 4096u); }
