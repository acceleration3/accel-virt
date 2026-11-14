#include "harness/framework/test_registry.hpp"
#include "proto/resources/cross_adapter_layout.hpp"

using av::CrossAdapterLayout;

TEST(cross_adapter_layout, a_texture_has_aligned_rows_and_an_aligned_height)
{ const CrossAdapterLayout layout(1249u, 805u, 21u, 4u);
  CHECK(layout.valid());
  CHECK_EQ(layout.pitch(), 5120u);
  CHECK(layout.bytes() == 1010u * 4096u);
  CHECK_EQ(layout.surface().width(), 1249u);
  CHECK_EQ(layout.surface().height(), 805u);
  CHECK_EQ(layout.surface().pitch(), 5120u);
  CHECK(layout.surface().bytes() <= layout.bytes());
  CHECK_EQ(CrossAdapterLayout(1056u, 16u, 21u, 4u).pitch(), 4352u);
  CHECK_EQ(CrossAdapterLayout(1056u, 16u, 21u, 4u).reported_pitch(), 4224u);
  CHECK_EQ(CrossAdapterLayout(160u, 120u, 21u, 4u).pitch(), 768u);
  CHECK_EQ(CrossAdapterLayout(160u, 120u, 21u, 4u).reported_pitch(), 640u);
  CHECK_EQ(CrossAdapterLayout(0u, 120u, 21u, 4u).reported_pitch(), 0u); }

TEST(cross_adapter_layout, a_heap_is_one_row_of_its_bytes)
{ const CrossAdapterLayout heap(0x30000u, 1u, 0u, 0u);
  CHECK(heap.valid());
  CHECK_EQ(heap.pitch(), 0x30000u);
  CHECK(heap.bytes() == 0x30000u);
  CHECK(!CrossAdapterLayout(0x1000u, 2u, 0u, 0u).valid());
  CHECK(!CrossAdapterLayout(0u, 1u, 0u, 0u).valid()); }

TEST(cross_adapter_layout, refuses_no_texels_and_sizes_past_32_bits)
{ CHECK(!CrossAdapterLayout(0u, 16u, 21u, 4u).valid());
  CHECK(!CrossAdapterLayout(16u, 0u, 21u, 4u).valid());
  CHECK(!CrossAdapterLayout(16u, 16u, 21u, 0u).valid());
  CHECK(!CrossAdapterLayout(0x40000000u, 1u, 21u, 4u).valid());
  CHECK(!CrossAdapterLayout(0x10000u, 0x4000u, 21u, 4u).valid()); }
