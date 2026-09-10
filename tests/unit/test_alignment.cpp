#include "../../src/core/alignment.hpp"
#include "../../src/core/platform.hpp"
#include "../harness/test_registry.hpp"

#include <cstdint>

using av::Alignment;

TEST(alignment, rejects_zero)
{ CHECK(!Alignment(0u).is_valid()); }

TEST(alignment, rejects_non_powers_of_two)
{ CHECK(!Alignment(3u).is_valid());
  CHECK(!Alignment(6u).is_valid());
  CHECK(!Alignment(4097u).is_valid()); }

TEST(alignment, accepts_powers_of_two)
{ CHECK(Alignment(1u).is_valid());
  CHECK(Alignment(4096u).is_valid());
  CHECK(Alignment(0x8000000000000000ull).is_valid()); }

TEST(alignment, reports_its_value)
{ CHECK_EQ(Alignment(4096u).value(), 4096u); }

TEST(alignment_divides, is_false_for_an_invalid_alignment)
{ CHECK(!Alignment(0u).divides(0u)); }

TEST(alignment_divides, distinguishes_aligned_from_unaligned)
{ const Alignment page(4096u);
  CHECK(page.divides(0u));
  CHECK(page.divides(8192u));
  CHECK(!page.divides(1u));
  CHECK(!page.divides(4095u)); }

TEST(alignment_align_up, fails_for_an_invalid_alignment)
{ std::uint64_t out = 0u; CHECK(!Alignment(5u).align_up(0u, out)); }

TEST(alignment_align_up, reports_overflow_instead_of_wrapping)
{ std::uint64_t out = 0u;
  const Alignment page(4096u);
  CHECK(!page.align_up(UINT64_MAX, out));
  CHECK(!page.align_up(UINT64_MAX - 4094u, out));
  CHECK(page.align_up(UINT64_MAX - 4095u, out));
  CHECK_EQ(out, UINT64_MAX - 4095u); }

TEST(alignment_align_up, rounds_up_and_leaves_aligned_values_alone)
{ std::uint64_t out = 0u;
  const Alignment page(4096u);
  CHECK(page.align_up(1u, out));    CHECK_EQ(out, 4096u);
  CHECK(page.align_up(4096u, out)); CHECK_EQ(out, 4096u);
  CHECK(page.align_up(0u, out));    CHECK_EQ(out, 0u); }
