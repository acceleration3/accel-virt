/* A surface point names a frame only with both halves: a surface without a
 * point, or a point of no surface, names nothing; equality is both halves. */
#include "harness/framework/test_registry.hpp"
#include "proto/resources/surface_point.hpp"

#include <cstdint>

using av::SurfacePoint;

TEST(surface_point, names_a_frame_only_with_a_surface_and_a_point)
{ const SurfacePoint none;
  CHECK_EQ(none.surface(), 0u);
  CHECK_EQ(none.point(), 0u);
  CHECK(!none.names());
  const SurfacePoint frame(0x0123456789ABCDEFull, 0xFEDCBA98u);
  CHECK_EQ(frame.surface(), 0x0123456789ABCDEFull);
  CHECK_EQ(frame.point(), 0xFEDCBA98u);
  CHECK(frame.names());
  CHECK(!SurfacePoint(5u, 0u).names());
  CHECK(!SurfacePoint(0u, 5u).names()); }

TEST(surface_point, is_equal_only_in_both_halves)
{ CHECK(SurfacePoint(4u, 9u) == SurfacePoint(4u, 9u));
  CHECK(!(SurfacePoint(4u, 9u) == SurfacePoint(5u, 9u)));
  CHECK(!(SurfacePoint(4u, 9u) == SurfacePoint(4u, 8u)));
  CHECK(SurfacePoint() == SurfacePoint(0u, 0u)); }
