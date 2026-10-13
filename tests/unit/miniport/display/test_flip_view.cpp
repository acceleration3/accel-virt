/* One flip as a vsync reports it: its kind and address, and for a base
 * plane the layer dxgmms2 compares with the flip - layer 0, enabled, at the
 * flipped address with the flip's own attributes, whatever the record held. */
#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info_record.hpp"
#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info2_record.hpp"
#include "miniport/display/flip_view.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::FlipView;

TEST(flip_view, a_flip_keeps_its_kind_address_and_attributes)
{ std::uint32_t attributes[av::multiplane_overlay_attribute_words] = {};
  attributes[0] = 3u;
  attributes[av::multiplane_overlay_attribute_words - 1u] = 9u;
  const FlipView flip(FlipView::Kind::base_plane, 0x6000000000u, attributes);
  attributes[0] = 99u;   /* the view took a copy */
  CHECK(flip.kind() == FlipView::Kind::base_plane);
  CHECK_EQ(flip.address(), 0x6000000000u);
  av::DxgkMultiplaneOverlayVsyncInfoRecord layer = {};
  layer.LayerIndex = 5u;
  flip.layer(layer);
  CHECK_EQ(layer.LayerIndex, 0u);
  CHECK_EQ(layer.Enabled, 1u);
  CHECK_EQ(layer.PhysicalAddress, 0x6000000000ll);
  CHECK_EQ(layer.PlaneAttributes[0], 3u);
  CHECK_EQ(layer.PlaneAttributes[av::multiplane_overlay_attribute_words - 1u], 9u); }

TEST(flip_view, a_flip_with_no_attributes_and_the_empty_view_carry_none)
{ const FlipView plain(FlipView::Kind::address, 0x5000u, nullptr);
  av::DxgkMultiplaneOverlayVsyncInfoRecord layer = {};
  layer.PlaneAttributes[4] = 7u;
  plain.layer(layer);
  CHECK_EQ(layer.PlaneAttributes[4], 0u);
  const FlipView none;
  CHECK(none.kind() == FlipView::Kind::address);
  CHECK_EQ(none.address(), 0u); }

/* An MPO3 flip is retired by its PresentId alone: the layer it reports is
 * layer 0 with that id, whether the plane was shown or turned off. */
TEST(flip_view, a_presented_plane_reports_layer_0_and_its_present_id)
{ const FlipView shown(0x6000000000u, 41u);
  CHECK(shown.kind() == FlipView::Kind::presented_plane);
  CHECK_EQ(shown.address(), 0x6000000000u);
  av::DxgkMultiplaneOverlayVsyncInfo2Record layer = {};
  layer.LayerIndex = 5u;
  layer.Flags = 1u;
  shown.layer(layer);
  CHECK_EQ(layer.LayerIndex, 0u);
  CHECK_EQ(layer.PresentId, 41u);
  CHECK_EQ(layer.Flags, 0u);
  const FlipView off(0u, 42u);
  off.layer(layer);
  CHECK_EQ(off.address(), 0u);
  CHECK_EQ(layer.PresentId, 42u); }
