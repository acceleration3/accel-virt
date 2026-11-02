#include "harness/framework/test_registry.hpp"
#include "icd/d3d/present_count.hpp"

#include <cstdint>
#include <cstring>

using av::d3d::PresentCount;

TEST(present_count, a_surface_never_presented_names_no_point)
{ const PresentCount count;
  CHECK_EQ(count.last(), 0u); }

TEST(present_count, each_present_publishes_the_next_point)
{ PresentCount count;
  CHECK_EQ(count.next(), 1u);
  CHECK_EQ(count.last(), 1u);
  CHECK_EQ(count.next(), 2u);
  CHECK_EQ(count.next(), 3u);
  CHECK_EQ(count.last(), 3u); }

TEST(present_count, wraps_past_the_last_point_to_one_never_to_zero)
{ static_assert(sizeof(PresentCount) == sizeof(std::uint32_t), "the count is the point it last published");
  PresentCount count;
  const std::uint32_t last_point = ~std::uint32_t{0};
  std::memcpy(&count, &last_point, sizeof last_point);
  CHECK_EQ(count.last(), last_point);
  CHECK_EQ(count.next(), 1u);
  CHECK_EQ(count.next(), 2u); }

TEST(present_count, a_copy_goes_on_from_the_original_s_point)
{ PresentCount count;
  static_cast<void>(count.next());
  static_cast<void>(count.next());
  PresentCount moved = count;
  CHECK_EQ(moved.next(), 3u);
  CHECK_EQ(count.last(), 2u); }
