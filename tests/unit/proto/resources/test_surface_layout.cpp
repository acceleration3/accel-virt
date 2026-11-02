#include "harness/framework/test_registry.hpp"
#include "proto/resources/surface_layout.hpp"

using av::SurfaceLayout;

TEST(surface_layout, remembers_its_fields)
{ const SurfaceLayout layout(640u, 480u, 21u, 2560u);
  CHECK_EQ(layout.width(), 640u);
  CHECK_EQ(layout.height(), 480u);
  CHECK_EQ(layout.format(), 21u);
  CHECK_EQ(layout.pitch(), 2560u); }

TEST(surface_layout_bytes, is_pitch_times_height_without_overflow)
{ CHECK(SurfaceLayout(1u, 0xFFFFFFFFu, 21u, 0xFFFFFFFFu).bytes() == 0xFFFFFFFE00000001ull);
  CHECK(SurfaceLayout().bytes() == 0u); }

TEST(surface_layout_is_surface, needs_width_height_and_pitch)
{ CHECK(SurfaceLayout(1u, 1u, 0u, 1u).is_surface());
  CHECK(!SurfaceLayout(0u, 1u, 0u, 1u).is_surface());
  CHECK(!SurfaceLayout(1u, 0u, 0u, 1u).is_surface());
  CHECK(!SurfaceLayout(1u, 1u, 0u, 0u).is_surface());
  CHECK(!SurfaceLayout().is_surface()); }

TEST(surface_layout_red_first, is_the_two_red_first_formats_and_no_other)
{ CHECK(SurfaceLayout(1u, 1u, 32u, 4u).red_first());
  CHECK(SurfaceLayout(1u, 1u, 33u, 4u).red_first());
  CHECK(!SurfaceLayout(1u, 1u, 21u, 4u).red_first());
  CHECK(!SurfaceLayout(1u, 1u, 22u, 4u).red_first());
  CHECK(!SurfaceLayout().red_first()); }
