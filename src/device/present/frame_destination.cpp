#include "device/present/frame_destination.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/dma/present_region.hpp"

namespace av {

FrameDestination::FrameDestination(int fd, uint64_t bytes, uint32_t pitch)
    : fd_(fd), bytes_(bytes), pitch_(pitch), regions_{}, count_(0u)
{
    AV_SCOPE("FrameDestination::FrameDestination");
}

bool FrameDestination::add(const PresentRegion &region)
{
    AV_SCOPE("FrameDestination::add");
    if (count_ == PresentCopyRequest::max_regions) return false;
    regions_[count_++] = AvFrameRegion{region.source_x(),      region.source_y(), region.destination_x(),
                                       region.destination_y(), region.width(),    region.height()};
    return true;
}

int FrameDestination::fd() const
{
    AV_SCOPE("FrameDestination::fd");
    return fd_;
}

uint64_t FrameDestination::bytes() const
{
    AV_SCOPE("FrameDestination::bytes");
    return bytes_;
}

uint32_t FrameDestination::pitch() const
{
    AV_SCOPE("FrameDestination::pitch");
    return pitch_;
}

const AvFrameRegion *FrameDestination::regions() const
{
    AV_SCOPE("FrameDestination::regions");
    return regions_;
}

uint32_t FrameDestination::count() const
{
    AV_SCOPE("FrameDestination::count");
    return count_;
}

}  // namespace av
