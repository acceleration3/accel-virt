#include "device/present/present_frames.hpp"

#include "core/trace/function_scope.hpp"
#include "device/context/context_plane.hpp"
#include "proto/dma/present_region.hpp"
#include "proto/resources/surface_pair.hpp"
#include "proto/resources/surface_point.hpp"

#include "proto/virtio_gpu/queued_present_request.hpp"

namespace av {

static_assert(PresentFrames::copy_regions == QueuedPresentRequest::max_regions, "a request carries every region the plane copies");

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

void PresentFrames::unmap(const SurfacePoint &frame) const
{
    AV_SCOPE("PresentFrames::unmap");
    if (readable()) plane_->unmap_frame(gpu_uuid_.raw(), frame);
}

bool PresentFrames::copies() const
{
    AV_SCOPE("PresentFrames::copies");
    return plane_ != nullptr && plane_->copies_surfaces();
}

bool PresentFrames::map_surface(uint64_t surface, AvFrameView &view) const
{
    AV_SCOPE("PresentFrames::map_surface");
    return copies() && plane_->map_surface(gpu_uuid_.raw(), surface, surface_wait_ms, view);
}

void PresentFrames::unmap_surface(uint64_t surface) const
{
    AV_SCOPE("PresentFrames::unmap_surface");
    if (copies()) plane_->unmap_surface(gpu_uuid_.raw(), surface);
}

bool PresentFrames::copy(const SurfacePair &surfaces, const PresentRegion *regions, uint32_t count) const
{
    AV_SCOPE("PresentFrames::copy");
    if (!copies() || regions == nullptr || count == 0u || count > copy_regions) return false;
    AvCopyRegion moved[copy_regions] = {};
    for (uint32_t index = 0u; index < count; index++)
        moved[index] = AvCopyRegion{regions[index].source_x(), regions[index].source_y(), regions[index].destination_x(),
                                    regions[index].destination_y(), regions[index].width(), regions[index].height()};
    return plane_->copy_surface(gpu_uuid_.raw(), surfaces, moved, count, copy_wait_ms);
}

}
