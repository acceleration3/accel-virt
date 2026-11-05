#include "harness/framework/test_registry.hpp"
#include "miniport/memory/point_view.hpp"

using av::PointView;

TEST(point_view, starts_as_nothing_named)
{ const PointView view;
  CHECK(!view.pointed());
  CHECK(!view.queued());
  CHECK_EQ(view.point(), 0u);
  CHECK(!view.first_starvation()); }

TEST(point_view, holds_what_it_was_made_of)
{ const PointView view(true, false, 0x55u, true);
  CHECK(view.pointed());
  CHECK(!view.queued());
  CHECK_EQ(view.point(), 0x55u);
  CHECK(view.first_starvation());
  const PointView queued(true, true, 3u, false);
  CHECK(queued.queued());
  CHECK(!queued.first_starvation()); }
