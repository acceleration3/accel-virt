#include "device/present/present_snapshots.hpp"

#include "core/trace/function_scope.hpp"
#include "device/context/context_plane.hpp"

namespace av {

PresentSnapshots::PresentSnapshots() : plane_(nullptr), gpu_uuid_()
{
    AV_SCOPE("PresentSnapshots::PresentSnapshots");
}

PresentSnapshots::PresentSnapshots(ContextPlane &plane, const uint8_t *gpu_uuid) : plane_(&plane), gpu_uuid_()
{
    AV_SCOPE("PresentSnapshots::PresentSnapshots");
    __builtin_memcpy(gpu_uuid_, gpu_uuid, uuid_bytes);
}

bool PresentSnapshots::readable() const
{
    AV_SCOPE("PresentSnapshots::readable");
    return plane_ != nullptr && plane_->reads_snapshots();
}

bool PresentSnapshots::map(uint64_t key, RvkSnapshotView &view) const
{
    AV_SCOPE("PresentSnapshots::map");
    return readable() && plane_->map_snapshot(gpu_uuid_, key, wait_ms, view);
}

void PresentSnapshots::unmap(uint64_t key) const
{
    AV_SCOPE("PresentSnapshots::unmap");
    if (readable()) plane_->unmap_snapshot(gpu_uuid_, key);
}

}  // namespace av
