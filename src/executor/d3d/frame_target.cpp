#include "executor/d3d/frame_target.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint64_t pixel_bytes = 4u;

}  // namespace

FrameTarget::FrameTarget(int fd, uint64_t bytes, uint32_t pitch, const AvFrameRegion *regions, uint32_t count)
    : fd_(fd), bytes_(bytes), pitch_(pitch), regions_(regions), count_(regions != nullptr ? count : 0u)
{
    AV_SCOPE("FrameTarget::FrameTarget");
}

int FrameTarget::fd() const
{
    AV_SCOPE("FrameTarget::fd");
    return fd_;
}

uint64_t FrameTarget::bytes() const
{
    AV_SCOPE("FrameTarget::bytes");
    return bytes_;
}

uint32_t FrameTarget::count() const
{
    AV_SCOPE("FrameTarget::count");
    return count_;
}

uint32_t FrameTarget::pitch() const
{
    AV_SCOPE("FrameTarget::pitch");
    return pitch_;
}

const AvFrameRegion &FrameTarget::region(uint32_t index) const
{
    AV_SCOPE("FrameTarget::region");
    return regions_[index];
}

bool FrameTarget::fits(uint32_t width, uint32_t height) const
{
    AV_SCOPE("FrameTarget::fits");
    if (fd_ < 0 || count_ == 0u || pitch_ == 0u || pitch_ % pixel_bytes != 0u) return false;
    for (uint32_t index = 0u; index < count_; index++)
        if (!inside(regions_[index], width, height)) return false;
    return true;
}

/* In 64 bits throughout: every field is the guest's, and a sum of two of them
 * must not wrap into range. */
bool FrameTarget::inside(const AvFrameRegion &region, uint32_t width, uint32_t height) const
{
    AV_SCOPE("FrameTarget::inside");
    if (region.width == 0u || region.height == 0u) return false;
    if (uint64_t{region.source_x} + region.width > width || uint64_t{region.source_y} + region.height > height)
        return false;
    const uint64_t row_end = (uint64_t{region.destination_x} + region.width) * pixel_bytes;
    if (row_end > pitch_) return false;
    const uint64_t last_row = uint64_t{region.destination_y} + region.height - 1u;
    return last_row * pitch_ + row_end <= bytes_;
}

VkBufferImageCopy FrameTarget::copy(uint32_t index) const
{
    AV_SCOPE("FrameTarget::copy");
    const AvFrameRegion &region = regions_[index];
    VkBufferImageCopy    copied = {};
    copied.bufferOffset = uint64_t{region.destination_y} * pitch_ + uint64_t{region.destination_x} * pixel_bytes;
    copied.bufferRowLength = pitch_ / static_cast<uint32_t>(pixel_bytes);
    copied.bufferImageHeight = 0u;
    copied.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
    copied.imageOffset = {static_cast<int32_t>(region.source_x), static_cast<int32_t>(region.source_y), 0};
    copied.imageExtent = {region.width, region.height, 1u};
    return copied;
}

}  // namespace av
