#include "harness/framework/test_registry.hpp"
#include "proto/resources/subresource_layout.hpp"

#include <cstdint>

using av::SubresourceLayout;

TEST(subresource_layout, counts_mips_by_layers_by_planes_and_holds_only_those)
{ const SubresourceLayout layout(3u, 4u, 2u);
  CHECK_EQ(layout.count(), 24u);
  CHECK(layout.holds(0u));
  CHECK(layout.holds(23u));
  CHECK(!layout.holds(24u));
  const SubresourceLayout overflowing(0xFFFFu, 0xFFFFu, 0xFFFFu);
  CHECK_EQ(overflowing.count(), 0u);
  CHECK(!overflowing.holds(0u));
  const SubresourceLayout wide(0x10000u, 0x10000u, 1u);
  CHECK_EQ(wide.count(), 0u);
  CHECK(!wide.holds(0u)); }

TEST(subresource_layout_mip_of, is_the_mip_a_subresource_index_names_in_each_layer)
{ const SubresourceLayout layout(3u, 4u, 1u);
  CHECK_EQ(layout.mip_of(0u), 0u);
  CHECK_EQ(layout.mip_of(2u), 2u);
  CHECK_EQ(layout.mip_of(3u), 0u);
  CHECK_EQ(layout.mip_of(11u), 2u);
  const SubresourceLayout none(0u, 4u, 1u);
  CHECK_EQ(none.mip_of(5u), 0u); }

TEST(subresource_layout_layer_of, is_the_layer_a_subresource_index_names_within_its_plane)
{ const SubresourceLayout layout(3u, 4u, 2u);
  CHECK_EQ(layout.layer_of(0u), 0u);
  CHECK_EQ(layout.layer_of(2u), 0u);
  CHECK_EQ(layout.layer_of(3u), 1u);
  CHECK_EQ(layout.layer_of(11u), 3u);
  CHECK_EQ(layout.layer_of(12u), 0u);
  CHECK_EQ(layout.layer_of(23u), 3u);
  const SubresourceLayout none(3u, 0u, 1u);
  CHECK_EQ(none.layer_of(5u), 0u); }

TEST(subresource_layout_layer_of, a_layout_with_no_mips_names_layer_zero)
{ const SubresourceLayout none(0u, 4u, 1u);
  CHECK_EQ(none.layer_of(5u), 0u); }
