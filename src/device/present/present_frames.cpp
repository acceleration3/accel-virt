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
    __builtin_memcpy(gpu_uuid_, gpu_uuid, uuid_bytes);
}

bool PresentFrames::readable() const
{
    AV_SCOPE("PresentFrames::readable");
    return plane_ != nullptr && plane_->reads_frames();
}

bool PresentFrames::map(const SurfacePoint &frame, AvFrameView &view) const
{
    AV_SCOPE("PresentFrames::map");
    return readable() && plane_->map_frame(gpu_uuid_, frame, wait_ms, view);
}

void PresentFrames::unmap(const SurfacePoint &frame) const
{
    AV_SCOPE("PresentFrames::unmap");
    if (readable()) plane_->unmap_frame(gpu_uuid_, frame);
}

}  // namespace av
