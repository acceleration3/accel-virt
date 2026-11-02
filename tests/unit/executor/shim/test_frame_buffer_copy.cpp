/* Where a frame's regions land in a lending's buffer, judged without a GPU: a
 * layout is made for regions inside the buffer and image, and refused - never
 * wrapped - for a pitch past an int, a row past the rows the buffer holds, an
 * offset that is not whole texels, a row that runs into the next, a frame that
 * is not four bytes a texel, and regions that overlap. */
#include "device/abi/av_executor.h"
#include "executor/shim/frame_buffer_copy.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace av {

struct FrameBufferCopyProbe {
    static bool place(const VkImageCreateInfo &image, const AvFrameCopy &target, VkBufferImageCopy *placed)
    {
        FrameBufferCopy copies;
        return copies.place(image, target, placed);
    }
};

}  // namespace av

namespace {

VkImageCreateInfo frame_image()
{
    VkImageCreateInfo image = {};
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.format = VK_FORMAT_B8G8R8A8_UNORM;
    image.extent = {64u, 64u, 1u};
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    return image;
}

AvFrameCopy copy_of(const AvFrameRegion *regions, std::uint32_t count, std::uint32_t pitch = 256u,
                    std::uint64_t offset = 0u, std::uint64_t size = 64u * 256u)
{
    return AvFrameCopy{-1, pitch, 1u, size, offset, regions, count, 0u, 0u};
}

}  // namespace

TEST(frame_buffer_copy_place, lays_regions_inside_the_buffer_by_row_and_texel)
{ const AvFrameRegion regions[2] = {{1u, 2u, 3u, 4u, 5u, 6u}, {0u, 0u, 10u, 0u, 4u, 1u}};
  VkBufferImageCopy   placed[2] = {};
  CHECK(av::FrameBufferCopyProbe::place(frame_image(), copy_of(regions, 2u, 256u, 4096u, 4096u + 64u * 256u), placed));
  CHECK_EQ(placed[0].bufferOffset, 4096u + 4u * 256u + 3u * 4u);
  CHECK_EQ(placed[0].bufferRowLength, 64u);
  CHECK_EQ(placed[0].imageOffset.x, 1);
  CHECK_EQ(placed[0].imageExtent.height, 6u);
  CHECK_EQ(placed[1].bufferOffset, 4096u + 10u * 4u); }

TEST(frame_buffer_copy_place, refuses_what_would_wrap_or_leave_the_buffer)
{ VkBufferImageCopy placed[2] = {};
  /* The wrap found in review: row 0xFFFFFFFF, 6 rows, 4 wide, pitch 0xFFFFFFFC. */
  const AvFrameRegion wrapped = {0u, 0u, 0u, 0xFFFFFFFFu, 4u, 6u};
  CHECK(!av::FrameBufferCopyProbe::place(frame_image(), copy_of(&wrapped, 1u, 0xFFFFFFFCu), placed));
  CHECK(!av::FrameBufferCopyProbe::place(frame_image(), copy_of(&wrapped, 1u), placed));   /* row far below */
  const AvFrameRegion past = {0u, 0u, 0u, 60u, 4u, 5u};   /* one row too many */
  CHECK(!av::FrameBufferCopyProbe::place(frame_image(), copy_of(&past, 1u), placed));
  const AvFrameRegion fits = {0u, 0u, 0u, 60u, 4u, 4u};
  CHECK(av::FrameBufferCopyProbe::place(frame_image(), copy_of(&fits, 1u), placed));
  CHECK(!av::FrameBufferCopyProbe::place(frame_image(), copy_of(&fits, 1u, 256u, 2u), placed));   /* not whole texels */
  CHECK(!av::FrameBufferCopyProbe::place(frame_image(), copy_of(&fits, 1u, 258u), placed));
  const AvFrameRegion spilling = {0u, 0u, 62u, 0u, 4u, 1u};   /* its row runs into the next */
  CHECK(!av::FrameBufferCopyProbe::place(frame_image(), copy_of(&spilling, 1u), placed));
  const AvFrameRegion outside = {62u, 0u, 0u, 0u, 4u, 1u};   /* leaves the frame */
  CHECK(!av::FrameBufferCopyProbe::place(frame_image(), copy_of(&outside, 1u), placed)); }

TEST(frame_buffer_copy_place, refuses_overlapping_regions_and_frames_that_are_not_four_bytes)
{ VkBufferImageCopy   placed[2] = {};
  const AvFrameRegion overlapping[2] = {{0u, 0u, 0u, 0u, 8u, 8u}, {0u, 0u, 7u, 7u, 8u, 8u}};
  CHECK(!av::FrameBufferCopyProbe::place(frame_image(), copy_of(overlapping, 2u), placed));
  const AvFrameRegion apart[2] = {{0u, 0u, 0u, 0u, 8u, 8u}, {0u, 0u, 8u, 8u, 8u, 8u}};
  CHECK(av::FrameBufferCopyProbe::place(frame_image(), copy_of(apart, 2u), placed));
  VkImageCreateInfo wide = frame_image();
  wide.format = VK_FORMAT_R16G16B16A16_UNORM;
  CHECK(!av::FrameBufferCopyProbe::place(wide, copy_of(apart, 2u), placed));
  VkImageCreateInfo multisampled = frame_image();
  multisampled.samples = VK_SAMPLE_COUNT_4_BIT;
  CHECK(!av::FrameBufferCopyProbe::place(multisampled, copy_of(apart, 2u), placed)); }
