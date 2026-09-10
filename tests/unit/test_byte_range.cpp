#include "../../src/core/byte_range.hpp"
#include "../../src/core/platform.hpp"
#include "../harness/test_registry.hpp"

using av::ByteRange;

TEST(byte_range, default_construction_is_not_representable)
{ CHECK(!ByteRange().is_representable()); }

TEST(byte_range, an_empty_span_is_not_representable)
{ CHECK(!ByteRange(4096u, 0u).is_representable()); }

TEST(byte_range, a_span_that_wraps_is_not_representable)
{ CHECK(!ByteRange(UINT64_MAX, 2u).is_representable());
  CHECK(!ByteRange(2u, UINT64_MAX).is_representable()); }

TEST(byte_range, a_span_ending_exactly_at_the_top_is_representable)
{ const ByteRange range(UINT64_MAX - 4096u, 4096u);
  CHECK(range.is_representable());
  CHECK(range.end() == UINT64_MAX); }

TEST(byte_range, a_span_ending_one_past_the_top_is_not)
{ CHECK(!ByteRange(UINT64_MAX - 4095u, 4096u).is_representable()); }

TEST(byte_range, reports_base_size_and_end)
{ const ByteRange range(4096u, 8192u);
  CHECK_EQ(range.base(), 4096u);
  CHECK_EQ(range.size(), 8192u);
  CHECK_EQ(range.end(), 12288u); }

TEST(byte_range_contains, excludes_below_and_at_the_end)
{ const ByteRange range(4096u, 4096u);
  CHECK(!range.contains(4095u));
  CHECK(range.contains(4096u));
  CHECK(range.contains(8191u));
  CHECK(!range.contains(8192u)); }

TEST(byte_range_encloses, rejects_unrepresentable_operands)
{ CHECK(!ByteRange().encloses(ByteRange(0u, 8u)));
  CHECK(!ByteRange(0u, 8u).encloses(ByteRange())); }

TEST(byte_range_encloses, rejects_an_inner_span_that_escapes_either_end)
{ const ByteRange outer(4096u, 8192u);
  CHECK(!outer.encloses(ByteRange(0u, 4096u)));
  CHECK(!outer.encloses(ByteRange(8192u, 8192u))); }

TEST(byte_range_encloses, accepts_an_exact_fit_and_a_strict_subset)
{ const ByteRange outer(4096u, 8192u);
  CHECK(outer.encloses(outer));
  CHECK(outer.encloses(ByteRange(8192u, 4096u))); }

TEST(byte_range, orders_by_base)
{ CHECK(ByteRange(0u, 1u).starts_below(ByteRange(1u, 1u)));
  CHECK(!ByteRange(1u, 1u).starts_below(ByteRange(1u, 1u))); }
