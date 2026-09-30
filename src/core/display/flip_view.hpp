/* One flip of one VidPn source as a vertical blank reports it: how the source
 * was flipped, the address it was flipped to and, for a multiplane flip of its
 * base plane, the plane's attributes. dxgmms2 retires a single-plane flip only
 * on a CRTC_VSYNC naming its address, and a multiplane one only on a
 * CRTC_VSYNC_WITH_MULTIPLANE_OVERLAY carrying the layer as the flip gave it
 * (VidSchiIsExpectedVSyncCookie, dxgmms2.sys @14003a0d4). */
#ifndef AV_FLIP_VIEW_HPP
#define AV_FLIP_VIEW_HPP

#include "core/ddi/dxgk_multiplane_overlay_plane_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkMultiplaneOverlayVsyncInfoRecord;

class FlipView {
public:
    enum class Kind : uint32_t {
        address = 0u,          /* SetVidPnSourceAddress */
        planes_disabled = 1u,  /* a multiplane flip with no enabled base plane: dxgmms2's disable-planes flip */
        base_plane = 2u,       /* a multiplane flip showing its base plane */
    };

    FlipView();
    FlipView(Kind kind, uint64_t address, const uint32_t *attributes);

    Kind     kind() const;
    uint64_t address() const;
    /* The layer a vsync reports for a base_plane flip: layer 0, enabled, at the
     * flipped address with the flip's attributes. */
    void     layer(DxgkMultiplaneOverlayVsyncInfoRecord &layer) const;

private:
    Kind     kind_;
    uint64_t address_;
    uint32_t attributes_[multiplane_overlay_attribute_words];
};

}  // namespace av

#endif  // AV_FLIP_VIEW_HPP
