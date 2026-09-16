#include "core/trace/image_range.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ImageRange;

namespace av {

struct ImageRangeProbe {
    static void wrap(ImageRange &image) { image.size_ = 0xFFFFFFFFFFFFFFFFull; }
};

}  // namespace av

TEST(image_range, contains_whole_spans_inside_it_only)
{ const ImageRange image(0x1000u, 0x100u);
  CHECK(image.contains(0x1000u, 1u));
  CHECK(image.contains(0x10FFu, 1u));
  CHECK(image.contains(0x1000u, 0x100u));
  CHECK(image.contains(0x1080u, 0u));
  CHECK(!image.contains(0x0FFFu, 1u));
  CHECK(!image.contains(0x1100u, 1u));
  CHECK(!image.contains(0x10FFu, 2u));
  CHECK(!image.contains(0x1000u, 0x101u));
  CHECK(!image.contains(0x1001u, 0xFFFFFFFFFFFFFFFFull));
  CHECK_EQ(image.end(), 0x1100u);
  CHECK(image.invariants_hold()); }

TEST(image_range, offsets_are_relative_inside_and_absolute_outside)
{ const ImageRange image(0x140000000u, 0x2000u);
  CHECK_EQ(image.offset_of(0x140001234u), 0x1234u);
  CHECK_EQ(image.offset_of(0x140000000u), 0u);
  CHECK_EQ(image.offset_of(0x7FF600001000u), 0x7FF600001000u); }

TEST(image_range, a_range_that_would_wrap_holds_nothing)
{ const ImageRange wrapped(0xFFFFFFFFFFFFF000ull, 0x2000u);
  CHECK(!wrapped.contains(0xFFFFFFFFFFFFF000ull, 1u));
  CHECK_EQ(wrapped.end(), 0xFFFFFFFFFFFFF000ull);
  CHECK(wrapped.invariants_hold());
  /* Ending exactly at 2^64 wraps its end to zero, so it is empty too. */
  const ImageRange top(0xFFFFFFFFFFFFF000ull, 0x1000u);
  CHECK(!top.contains(0xFFFFFFFFFFFFFFFFull, 1u));
  CHECK_EQ(top.end(), 0xFFFFFFFFFFFFF000ull);
  const ImageRange below(0xFFFFFFFFFFFFE000ull, 0x1000u);
  CHECK(below.contains(0xFFFFFFFFFFFFEFFFull, 1u)); }

TEST(image_range_invariants, fail_for_a_range_that_wraps)
{ ImageRange image(0x1000u, 0x100u);
  av::ImageRangeProbe::wrap(image);
  CHECK(!image.invariants_hold()); }
