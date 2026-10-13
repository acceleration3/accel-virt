/* A display source's overlay planes carried to the host as SCANOUT_PLANE
 * requests (docs/protocol.md, "Scanout planes"), for a device that composes
 * them: each layer's image by its host key, its source and destination
 * rectangles with the plane's clip and the head's edges folded in, and
 * whether it is shown. A layer is sent only when what it shows changed since
 * it was last sent, so a steady desktop sends nothing; the base frame then
 * follows as ever (ScanoutResource), and the device composes what the planes
 * say over it. */
#ifndef AV_PLANE_SCANOUT_HPP
#define AV_PLANE_SCANOUT_HPP

#include "miniport/display/display_sources.hpp"
#include "miniport/display/overlay_plane.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "proto/virtio_gpu/scanout_plane_request.hpp"

#include <stdint.h>

namespace av {

class VirtioGpuDevice;
struct PlaneScanoutProbe;

class PlaneScanout {
public:
    explicit PlaneScanout(ObjectStorage<VirtioGpuDevice> &device);
    PlaneScanout(const PlaneScanout &) = delete;
    PlaneScanout &operator=(const PlaneScanout &) = delete;

    /* `planes` (OverlayPlane::max_planes, by layer) of `source`, a `width` by
     * `height` head, sent where they changed. Up to DISPATCH. */
    [[nodiscard]] Status carry(uint32_t source, const OverlayPlane *planes, uint32_t width, uint32_t height);
    /* A new device has been told nothing: every layer is sent again. */
    void                 forget_device();

    /* What a plane shows of a `width` by `height` head: its request, turned
     * off where its clip leaves nothing of it on the head. */
    ScanoutPlaneRequest  request(uint32_t source, const OverlayPlane &plane, uint32_t width, uint32_t height) const;

private:
    ObjectStorage<VirtioGpuDevice> &device_;
    ScanoutPlaneRequest             sent_[DisplaySources::max_sources][OverlayPlane::max_planes];
    bool                            told_[DisplaySources::max_sources][OverlayPlane::max_planes];

    friend struct PlaneScanoutProbe;
};

}  // namespace av

#endif  // AV_PLANE_SCANOUT_HPP
