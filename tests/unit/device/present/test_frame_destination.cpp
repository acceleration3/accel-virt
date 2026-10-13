/* What the present engine hands the executor to write a frame into: the
 * dma-buf, the surface's length and pitch, and each region as the executor
 * names it - up to max_regions, and not one more. */
#include "device/present/frame_destination.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/dma/present_region.hpp"

#include <cstdint>

using av::FrameDestination;
using av::PresentRegion;

TEST(frame_destination, carries_the_dma_buf_and_each_region_as_the_executor_names_it)
{ FrameDestination destination(7, 8192u, 256u);
  CHECK_EQ(destination.fd(), 7);
  CHECK_EQ(destination.bytes(), 8192u);
  CHECK_EQ(destination.pitch(), 256u);
  CHECK_EQ(destination.count(), 0u);
  CHECK(destination.add(PresentRegion(1u, 2u, 3u, 4u, 5u, 6u)));
  CHECK_EQ(destination.count(), 1u);
  const AvFrameRegion &region = destination.regions()[0];
  CHECK_EQ(region.source_x, 1u);
  CHECK_EQ(region.source_y, 2u);
  CHECK_EQ(region.destination_x, 3u);
  CHECK_EQ(region.destination_y, 4u);
  CHECK_EQ(region.width, 5u);
  CHECK_EQ(region.height, 6u); }

TEST(frame_destination, holds_as_many_regions_as_a_present_carries_and_no_more)
{ FrameDestination destination(7, 8192u, 256u);
  for (std::uint32_t index = 0u; index < av::PresentCopyRequest::max_regions; index++)
      CHECK(destination.add(PresentRegion(0u, 0u, 0u, 0u, 1u, 1u)));
  CHECK(!destination.add(PresentRegion(0u, 0u, 0u, 0u, 1u, 1u)));
  CHECK_EQ(destination.count(), av::PresentCopyRequest::max_regions); }
