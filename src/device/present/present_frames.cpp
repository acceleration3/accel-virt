#include "device/present/present_frames.hpp"

#include "core/trace/function_scope.hpp"
#include "device/context/context_plane.hpp"
#include "proto/resources/surface_point.hpp"

namespace av {

PresentFrames::PresentFrames() : plane_(nullptr), gpu_uuid_()
{
    AV_SCOPE("PresentFrames::PresentFrames");
}

PresentFrames::PresentFrames(ContextPlane &plane, const uint8_t *gpu_uuid) : plane_(&plane), gpu_uuid_()
{
    AV_SCOPE("PresentFrames::PresentFrames");
    gpu_uuid_ = DeviceUuid(gpu_uuid);
}

bool PresentFrames::readable() const
{
    AV_SCOPE("PresentFrames::readable");
    return plane_ != nullptr && plane_->reads_frames();
}

bool PresentFrames::map(const SurfacePoint &frame, AvFrameView &view) const
{
    AV_SCOPE("PresentFrames::map");
    return readable() && plane_->map_frame(gpu_uuid_.raw(), frame, wait_ms, view);
}

bool PresentFrames::write(const SurfacePoint &frame, const FrameDestination &destination) const
{
    AV_SCOPE("PresentFrames::write");
    return readable() && plane_->write_frame(gpu_uuid_.raw(), frame, wait_ms, destination);
}

void PresentFrames::unmap(const SurfacePoint &frame) const
{
    AV_SCOPE("PresentFrames::unmap");
    if (readable()) plane_->unmap_frame(gpu_uuid_.raw(), frame);
}

}  // namespace av
