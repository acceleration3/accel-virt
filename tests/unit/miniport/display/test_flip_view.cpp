/* One flip as a vsync reports it: its kind and address, and for a base
 * plane the layer dxgmms2 compares with the flip - layer 0, enabled, at the
 * flipped address with the flip's own attributes, whatever the record held. */
#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info_record.hpp"
#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info2_record.hpp"
#include "miniport/ddi/dxgk_multiplane_overlay_attributes3_record.hpp"
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

/* An MPO3 flip is retired by its PresentIds alone: it reports every layer a
 * flip ever named, with its PresentId, shown or turned off, and skips a layer
 * never flipped. Its address is the base plane's. */
TEST(flip_view, a_presented_flip_reports_every_flipped_layer_and_its_present_id)
{ int allocation = 0;
  av::DxgkMultiplaneOverlayAttributes3Record attributes = {};
  av::OverlayPlane planes[av::OverlayPlane::max_planes];
  planes[0] = av::OverlayPlane(0u, 41u, &allocation, 0x6000000000ll, attributes);
  planes[2] = av::OverlayPlane(2u, 7u);   /* turned off */
  const FlipView flip(planes);
  CHECK(flip.kind() == FlipView::Kind::presented_plane);
  CHECK_EQ(flip.address(), 0x6000000000u);
  CHECK(flip.plane(0u).allocation() == &allocation);
  CHECK(!flip.plane(2u).shown());
  av::DxgkMultiplaneOverlayVsyncInfo2Record layers[av::OverlayPlane::max_planes] = {};
  layers[1].Flags = 1u;
  CHECK_EQ(flip.layers(layers), 2u);
  CHECK_EQ(layers[0].LayerIndex, 0u);
  CHECK_EQ(layers[0].PresentId, 41u);
  CHECK_EQ(layers[1].LayerIndex, 2u);
  CHECK_EQ(layers[1].PresentId, 7u);
  CHECK_EQ(layers[1].Flags, 0u);
  const FlipView none;
  CHECK_EQ(none.layers(layers), 0u); }
