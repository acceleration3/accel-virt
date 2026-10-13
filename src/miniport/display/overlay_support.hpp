/* What this adapter's overlay planes can show, as dxgkrnl asks before it
 * flips them: the caps (DxgkDdiGetMultiPlaneOverlayCaps,
 * DxgkDdiGetPostCompositionCaps) and the verdict on a configuration
 * (DxgkDdiCheckMultiPlaneOverlaySupport3). The host composes the planes into
 * the display's image (docs/protocol.md "Scanout planes"), so a plane is
 * supported when the host holds its image - a surface a Direct3D session
 * shares under a key - in a format the scanout takes, unrotated, in the RGB
 * colour space the display is, and scaled no further than the host's filter
 * keeps legible. */
#ifndef AV_OVERLAY_SUPPORT_HPP
#define AV_OVERLAY_SUPPORT_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

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
};

}  // namespace av

#endif  // AV_OVERLAY_SUPPORT_HPP
