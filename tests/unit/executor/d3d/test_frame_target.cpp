/* A frame's target as the guest named it, judged before the GPU writes:
 * every region inside the frame and inside the surface, the sums of the
 * guest's fields taken in 64 bits so none wraps into range, and each region
 * becomes the copy Vulkan makes into the surface at its pitch. */
#include "executor/d3d/frame_target.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::FrameTarget;

namespace {

constexpr std::uint32_t pitch = 64u;   /* 16 pixels */
constexpr std::uint64_t bytes = 8u * pitch;

}  // namespace

TEST(frame_target, a_region_inside_the_frame_and_the_surface_fits_and_becomes_its_copy)
{ const AvFrameRegion regions[] = {{1u, 2u, 3u, 4u, 5u, 2u}};
  const FrameTarget target(5, bytes, pitch, regions, 1u);
  CHECK_EQ(target.fd(), 5);
  CHECK_EQ(target.bytes(), bytes);
  CHECK_EQ(target.count(), 1u);
  CHECK(target.fits(8u, 8u));
  const VkBufferImageCopy copy = target.copy(0u);
  CHECK_EQ(copy.bufferOffset, 4u * pitch + 3u * 4u);
  CHECK_EQ(copy.bufferRowLength, 16u);
  CHECK_EQ(copy.imageOffset.x, 1);
  CHECK_EQ(copy.imageOffset.y, 2);
  CHECK_EQ(copy.imageExtent.width, 5u);
  CHECK_EQ(copy.imageExtent.height, 2u);
  /* Exactly to the edges fits: the last row ends at the surface's last byte. */
  const AvFrameRegion edge[] = {{0u, 0u, 0u, 7u, 16u, 1u}};
  CHECK(FrameTarget(5, bytes, pitch, edge, 1u).fits(16u, 1u)); }

TEST(frame_target, a_region_past_any_edge_or_one_that_wraps_does_not_fit)
{ const AvFrameRegion outside[] = {
      {7u, 0u, 0u, 0u, 2u, 1u},                     /* past the frame's width */
      {0u, 7u, 0u, 0u, 1u, 2u},                     /* past the frame's height */
      {0u, 0u, 15u, 0u, 2u, 1u},                    /* past a row of the surface */
      {0u, 0u, 0u, 7u, 1u, 2u},                     /* past the surface's last row */
      {0u, 0u, 0u, 0u, 0u, 1u},                     /* empty */
      {0u, 0u, 0u, 0u, 1u, 0u},
      {0xFFFFFFFFu, 0u, 0u, 0u, 2u, 1u},            /* sums that would wrap in 32 bits */
      {0u, 0u, 0xFFFFFFFFu, 0u, 2u, 1u},
      {0u, 0u, 0u, 0xFFFFFFFFu, 1u, 2u}};
  for (const AvFrameRegion &region : outside) CHECK(!FrameTarget(5, bytes, pitch, &region, 1u).fits(8u, 8u));
  const AvFrameRegion fine[] = {{0u, 0u, 0u, 0u, 1u, 1u}};
  CHECK(!FrameTarget(-1, bytes, pitch, fine, 1u).fits(8u, 8u));      /* no dma-buf */
  CHECK(!FrameTarget(5, bytes, 0u, fine, 1u).fits(8u, 8u));          /* no pitch */
  CHECK(!FrameTarget(5, bytes, pitch + 2u, fine, 1u).fits(8u, 8u));  /* not whole pixels */
  CHECK(!FrameTarget(5, bytes, pitch, fine, 0u).fits(8u, 8u));       /* nothing to write */
  CHECK(!FrameTarget(5, bytes, pitch, nullptr, 3u).fits(8u, 8u));
  CHECK_EQ(FrameTarget(5, bytes, pitch, nullptr, 3u).count(), 0u);
  /* One region of many outside refuses them all. */
  const AvFrameRegion mixed[] = {{0u, 0u, 0u, 0u, 1u, 1u}, {0u, 0u, 0u, 8u, 1u, 1u}};
  CHECK(!FrameTarget(5, bytes, pitch, mixed, 2u).fits(8u, 8u)); }
