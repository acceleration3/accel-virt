/* What this adapter's overlay planes can show, as dxgkrnl asks before it
 * flips them: the caps (DxgkDdiGetMultiPlaneOverlayCaps,
 * DxgkDdiGetPostCompositionCaps) and the verdict on a configuration
 * (DxgkDdiCheckMultiPlaneOverlaySupport3). The host composes the planes into
 * the display's image (docs/protocol.md "Scanout planes"), so a plane is
 * supported when the host holds its image - a surface a Direct3D session
 * shares under a key - unrotated and unmirrored, scaled no further than the
 * host's filter keeps legible, and either 32-bit RGB in the display's colour
 * space or NV12 or YUY2 video in a BT.601 or BT.709 YCbCr space over the base
 * plane, which the host converts as it composes. */
#ifndef AV_OVERLAY_SUPPORT_HPP
#define AV_OVERLAY_SUPPORT_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class CapsetInfo;
class DisplaySources;
struct DxgkargCheckMultiPlaneOverlaySupport3Record;
struct DxgkargGetMultiPlaneOverlayCapsRecord;
struct DxgkargGetPostCompositionCapsRecord;
struct DxgkMultiplaneOverlayPlaneWithSource2Record;
struct DxgkMultiplaneOverlayPostCompositionWithSourceRecord;

class OverlaySupport {
public:
    /* How far a plane may be stretched or shrunk, along either axis. */
    static constexpr uint32_t max_scale = 8u;

    OverlaySupport(Platform &platform, DisplaySources &sources);
    OverlaySupport(const OverlaySupport &) = delete;
    OverlaySupport &operator=(const OverlaySupport &) = delete;

    /* Whether the device composes planes, which the capset says at
     * StartDevice: one that does not shows the base plane alone, so that is
     * the one plane offered, and a configuration with any other refused. */
    void offer(const CapsetInfo &capset);
    /* The planes a source has: OverlayPlane::max_planes, or 1. */
    uint32_t planes() const;

    /* invalid_parameter for a source the adapter does not have. */
    [[nodiscard]] Status caps(DxgkargGetMultiPlaneOverlayCapsRecord &caps) const;
    /* The whole frame is never scaled after composition: 1 either way. */
    [[nodiscard]] Status post_composition_caps(DxgkargGetPostCompositionCapsRecord &caps) const;
    /* Supported, or not with the first plane that cannot be shown named in
     * ReturnInfo.FailingPlane. Only a malformed request is refused. */
    [[nodiscard]] Status check(DxgkargCheckMultiPlaneOverlaySupport3Record &check) const;

private:
    bool shows(const DxgkMultiplaneOverlayPlaneWithSource2Record &plane) const;
    bool shows(const DxgkMultiplaneOverlayPostCompositionWithSourceRecord &composition) const;

    Platform       &platform_;
    DisplaySources &sources_;
    bool            composed_;
};

}  // namespace av

#endif  // AV_OVERLAY_SUPPORT_HPP
