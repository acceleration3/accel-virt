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

    [[nodiscard]] Status carry(uint32_t source, const OverlayPlane *planes, uint32_t width, uint32_t height);
    void                 forget_device();

    ScanoutPlaneRequest  request(uint32_t source, const OverlayPlane &plane, uint32_t width, uint32_t height) const;

private:
    ObjectStorage<VirtioGpuDevice> &device_;
    ScanoutPlaneRequest             sent_[DisplaySources::max_sources][OverlayPlane::max_planes];
    bool                            told_[DisplaySources::max_sources][OverlayPlane::max_planes];

    friend struct PlaneScanoutProbe;
};

}

#endif
