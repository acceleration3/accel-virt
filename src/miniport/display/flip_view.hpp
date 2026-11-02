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
        address = 0u,
        planes_disabled = 1u,
        base_plane = 2u,
        presented_plane = 3u,
    };

    FlipView();
    FlipView(Kind kind, uint64_t address, const uint32_t *attributes);
    explicit FlipView(const OverlayPlane *planes);

    Kind     kind() const;
    uint64_t address() const;
    void     layer(DxgkMultiplaneOverlayVsyncInfoRecord &layer) const;
    uint32_t layers(DxgkMultiplaneOverlayVsyncInfo2Record *layers) const;
    const OverlayPlane &plane(uint32_t layer) const;

private:
    Kind     kind_;
    uint64_t address_;
    OverlayPlane planes_[OverlayPlane::max_planes];
    uint32_t attributes_[multiplane_overlay_attribute_words];
};

}

#endif
