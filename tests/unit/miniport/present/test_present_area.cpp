#include "miniport/present/present_area.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/dma/rectangle.hpp"

#include <cstdint>

using av::PresentArea;
using av::Rectangle;

namespace {

constexpr std::int64_t lowest = -2147483648;
constexpr std::int64_t highest = 2147483647;

}

TEST(present_area, an_area_is_the_rectangle_it_was_made_from)
{ const PresentArea area(Rectangle(-4, -8, 12, 16));
  CHECK_EQ(area.left(), -4);
  CHECK_EQ(area.top(), -8);
  CHECK_EQ(area.right(), 12);
  CHECK_EQ(area.bottom(), 16);
  CHECK_EQ(area.width(), 16);
  CHECK_EQ(area.height(), 24);
  const PresentArea widest(Rectangle(-2147483647 - 1, -2147483647 - 1, 2147483647, 2147483647));
  CHECK_EQ(widest.width(), highest - lowest);
  CHECK_EQ(widest.height(), highest - lowest);
  const PresentArea crossed(8, 8, 4, 4);
  CHECK_EQ(crossed.width(), 0);
  CHECK_EQ(crossed.height(), 0); }

TEST(present_area, clipping_to_a_surface_keeps_only_what_is_on_it)
{ const PresentArea over(-4, -4, 12, 12);
  const PresentArea kept = over.clipped_to(8u, 6u);
  CHECK_EQ(kept.left(), 0);
  CHECK_EQ(kept.top(), 0);
  CHECK_EQ(kept.right(), 8);
  CHECK_EQ(kept.bottom(), 6);
  const PresentArea beyond = PresentArea(20, 20, 30, 30).clipped_to(8u, 6u);
  CHECK_EQ(beyond.width(), 0);
  CHECK_EQ(beyond.height(), 0);
  CHECK_EQ(beyond.right(), beyond.left());
  CHECK_EQ(beyond.bottom(), beyond.top());
  const PresentArea before = PresentArea(-30, -30, -20, -20).clipped_to(8u, 6u);
  CHECK_EQ(before.width(), 0);
  CHECK_EQ(before.height(), 0);
  const PresentArea extreme = PresentArea(lowest, lowest, highest, highest).clipped_to(8u, 6u);
  CHECK_EQ(extreme.left(), 0);
  CHECK_EQ(extreme.right(), 8);
  CHECK_EQ(extreme.bottom(), 6);
  CHECK_EQ(over.clipped_to(0u, 0u).width(), 0); }

TEST(present_area, an_area_intersected_with_another_is_what_they_share)
{ const PresentArea rectangle(0, 0, 16, 16);
  const PresentArea shared = PresentArea(8, 4, 24, 12).intersected_with(rectangle);
  CHECK_EQ(shared.left(), 8);
  CHECK_EQ(shared.top(), 4);
  CHECK_EQ(shared.right(), 16);
  CHECK_EQ(shared.bottom(), 12);
  const PresentArea apart = PresentArea(20, 20, 24, 24).intersected_with(rectangle);
  CHECK_EQ(apart.width(), 0);
  CHECK_EQ(apart.height(), 0);
  const PresentArea inside = PresentArea(4, 4, 8, 8).intersected_with(rectangle);
  CHECK_EQ(inside.left(), 4);
  CHECK_EQ(inside.right(), 8);
  const PresentArea around = PresentArea(lowest, lowest, highest, highest).intersected_with(rectangle);
  CHECK_EQ(around.left(), 0);
  CHECK_EQ(around.right(), 16);
  CHECK_EQ(around.bottom(), 16); }
