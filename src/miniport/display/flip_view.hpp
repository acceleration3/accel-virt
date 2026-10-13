/* One flip of one VidPn source as a vertical blank reports it: how the source
 * was flipped, the address it was flipped to and, for a multiplane flip of its
 * base plane, the plane's attributes. dxgmms2 retires a single-plane flip only
 * on a CRTC_VSYNC naming its address, and a multiplane one only on a
 * CRTC_VSYNC_WITH_MULTIPLANE_OVERLAY carrying the layer as the flip gave it
 * (VidSchiIsExpectedVSyncCookie, dxgmms2.sys @14003a0d4). A flip made through
 * the MPO3 DDI retires on a CRTC_VSYNC_WITH_MULTIPLANE_OVERLAY2 naming each
 * layer's PresentId instead, so that flip keeps every plane of the source as it
 * left them. */
#ifndef AV_FLIP_VIEW_HPP
#define AV_FLIP_VIEW_HPP

#include "miniport/ddi/dxgk_multiplane_overlay_plane_record.hpp"
#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info2_record.hpp"
#include "miniport/display/overlay_plane.hpp"

#include <stdint.h>

namespace av {

struct DxgkMultiplaneOverlayVsyncInfoRecord;

class FlipView {
public:
    enum class Kind : uint32_t {
        address = 0u,          /* SetVidPnSourceAddress */
        planes_disabled = 1u,  /* a multiplane flip with no enabled base plane: dxgmms2's disable-planes flip */
        base_plane = 2u,       /* a multiplane flip showing its base plane */
        presented_plane = 3u,  /* an MPO3 flip: every plane of the source, shown or turned off */
    };

    FlipView();
    FlipView(Kind kind, uint64_t address, const uint32_t *attributes);
    /* A presented_plane flip: the source's planes, OverlayPlane::max_planes of
     * them indexed by layer. The address is the base plane's, 0 for none. */
    explicit FlipView(const OverlayPlane *planes);

    Kind     kind() const;
    uint64_t address() const;
    /* The layer a vsync reports for a base_plane flip: layer 0, enabled, at the
     * flipped address with the flip's attributes. */
    void     layer(DxgkMultiplaneOverlayVsyncInfoRecord &layer) const;
    /* The layers a vsync reports for a presented_plane flip, into `layers`,
     * which holds OverlayPlane::max_planes: every layer a flip ever named, with
     * its PresentId, shown or turned off. The count written. */
    uint32_t layers(DxgkMultiplaneOverlayVsyncInfo2Record *layers) const;
    /* A presented_plane flip's plane at `layer`; no plane for another kind. */
    const OverlayPlane &plane(uint32_t layer) const;

private:
    Kind     kind_;
    uint64_t address_;
    OverlayPlane planes_[OverlayPlane::max_planes];
    uint32_t attributes_[multiplane_overlay_attribute_words];
};

}  // namespace av

#endif  // AV_FLIP_VIEW_HPP
