#include "harness/framework/test_registry.hpp"
#include "icd/opengl/pixel_format_criteria.hpp"

#include <cstdint>

using av::glicd::PixelFormatCriteria;

namespace av::glicd {

struct PixelFormatCriteriaProbe {
    static void zero_name(PixelFormatCriteria &criteria) { criteria.names_[0] = 0u; }
    static void overcount(PixelFormatCriteria &criteria) { criteria.count_ = PixelFormatCriteria::capacity + 1u; }
};

}  // namespace av::glicd

TEST(pixel_format_criteria, keeps_criteria_in_order_up_to_its_capacity)
{ PixelFormatCriteria criteria;
  CHECK_EQ(criteria.count(), 0u);
  CHECK_EQ(criteria.name(0u), 0u);
  CHECK(criteria.value(0u) == 0.0);
  for (std::uint32_t at = 0u; at < PixelFormatCriteria::capacity; at++) {
    CHECK(criteria.add(0x2000u + at, 0.5 + at));
    CHECK(criteria.invariants_hold());
  }
  CHECK(!criteria.add(0x3000u, 9.0));
  CHECK_EQ(criteria.count(), PixelFormatCriteria::capacity);
  CHECK_EQ(criteria.name(0u), 0x2000u);
  CHECK(criteria.value(255u) == 255.5);
  CHECK_EQ(criteria.name(PixelFormatCriteria::capacity), 0u);
  CHECK(criteria.value(PixelFormatCriteria::capacity) == 0.0);
  CHECK(criteria.invariants_hold()); }

TEST(pixel_format_criteria_invariants, fail_for_a_zero_name_or_a_count_past_capacity)
{ PixelFormatCriteria zeroed;
  CHECK(zeroed.add(0x2022u, 24.0));
  av::glicd::PixelFormatCriteriaProbe::zero_name(zeroed);
  CHECK(!zeroed.invariants_hold());
  PixelFormatCriteria overfull;
  av::glicd::PixelFormatCriteriaProbe::overcount(overfull);
  CHECK(!overfull.invariants_hold()); }
