#include "harness/framework/test_registry.hpp"
#include "proto/resources/surface_layout.hpp"

using av::SurfaceLayout;

TEST(surface_layout, remembers_its_fields)
{ const SurfaceLayout layout(640u, 480u, 21u, 2560u);
  CHECK_EQ(layout.width(), 640u);
  CHECK_EQ(layout.height(), 480u);
  CHECK_EQ(layout.format(), 21u);
  CHECK_EQ(layout.pitch(), 2560u); }

TEST(surface_layout_pages, round_a_part_page_up_and_a_whole_one_not)
{ CHECK(SurfaceLayout().pages() == 0u);
  CHECK(SurfaceLayout(1u, 1u, 21u, 1u).pages() == 1u);
  CHECK(SurfaceLayout(1024u, 1u, 21u, 4096u).pages() == 1u);
  CHECK(SurfaceLayout(1024u, 2u, 21u, 4097u).pages() == 3u);
  CHECK(SurfaceLayout(1298u, 847u, 22u, 5192u).pages() == 1074u);
  CHECK(SurfaceLayout(1u, 0xFFFFFFFFu, 21u, 0xFFFFFFFFu).pages() == 0xFFFFFFFE00000001ull / 4096u + 1u); }

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
