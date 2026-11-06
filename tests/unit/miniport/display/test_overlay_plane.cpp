#include "miniport/ddi/dxgk_multiplane_overlay_attributes3_record.hpp"
#include "miniport/display/overlay_plane.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::OverlayPlane;

TEST(overlay_plane, a_plane_never_flipped_is_neither_flipped_nor_shown)
{ const OverlayPlane none;
  CHECK(!none.flipped());
  CHECK(!none.shown());
  CHECK(none.allocation() == nullptr);
  CHECK_EQ(none.layer(), 0u);
  CHECK_EQ(none.present_id(), 0u); }

TEST(overlay_plane, a_flip_that_names_no_allocation_is_flipped_and_hidden)
{ const OverlayPlane hidden(2u, 17u);
  CHECK(hidden.flipped());
  CHECK(!hidden.shown());
  CHECK_EQ(hidden.layer(), 2u);
  CHECK_EQ(hidden.present_id(), 17u); }

TEST(overlay_plane, an_allocation_without_a_present_id_is_still_a_shown_flip)
{ int allocation = 0;
  av::DxgkMultiplaneOverlayAttributes3Record attributes = {};
  const OverlayPlane plane(1u, 0u, &allocation, 0x4000, attributes);
  CHECK(plane.flipped());
  CHECK(plane.shown());
  CHECK(plane.allocation() == &allocation);
  CHECK_EQ(plane.address(), 0x4000);
  CHECK_EQ(plane.flips(), 0u);
  CHECK(!plane.blended());
  CHECK(!plane.filtered()); }

TEST(overlay_plane, the_attributes_decide_the_rectangles_flips_blend_filter_and_colour_space)
{ int allocation = 0;
  av::DxgkMultiplaneOverlayAttributes3Record attributes = {};
  attributes.SrcRect = {1, 2, 3, 4};
  attributes.DstRect = {5, 6, 7, 8};
  attributes.ClipRect = {9, 10, 11, 12};
  attributes.Flags = av::overlay_vertical_flip;
  attributes.Blend = av::overlay_blend_alpha;
  attributes.StretchQuality = 1u;
  attributes.ColorSpaceType = 8u;
  const OverlayPlane plane(3u, 5u, &allocation, 0, attributes);
  CHECK_EQ(plane.source().right, 3);
  CHECK_EQ(plane.destination().bottom, 8);
  CHECK_EQ(plane.clip().left, 9);
  CHECK_EQ(plane.flips(), av::overlay_vertical_flip);
  CHECK(plane.blended());
  CHECK(plane.filtered());
  CHECK_EQ(plane.colour_space(), 8u); }
