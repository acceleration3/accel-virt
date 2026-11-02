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

Status PresentFrames::copy(const SurfacePoint &frame, const AvFrameCopy &copy) const
{
    AV_SCOPE("PresentFrames::copy");
    if (!readable()) return Status::not_supported;
    return plane_->copy_frame(gpu_uuid_.raw(), frame, wait_ms, copy);
}

void PresentFrames::unmap(const SurfacePoint &frame) const
{
    AV_SCOPE("PresentFrames::unmap");
    if (readable()) plane_->unmap_frame(gpu_uuid_.raw(), frame);
}

}
