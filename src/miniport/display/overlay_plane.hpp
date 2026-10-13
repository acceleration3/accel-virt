/* One overlay plane of a display source as an MPO3 flip left it: the layer, the
 * allocation it shows and where, how it is cut, placed and blended, and the
 * PresentId the flip gave it, which the vsync after it names. A plane the flip
 * turned off keeps its layer and PresentId, since that flip retires only on a
 * vsync naming them too (VidSchiIsExpectedVSyncCookie, dxgmms2.sys). */
#ifndef AV_OVERLAY_PLANE_HPP
#define AV_OVERLAY_PLANE_HPP

#include "miniport/ddi/rect_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkMultiplaneOverlayAttributes3Record;

class OverlayPlane {
public:
    /* The planes a source scans out, the base one included: what
     * DxgkDdiGetMultiPlaneOverlayCaps reports and DXGK_DRIVERCAPS.MaxOverlayPlanes
     * says. dxgkrnl refuses more than ten. */
    static constexpr uint32_t max_planes = 4u;

    /* No plane: never flipped. */
    OverlayPlane();
    /* A plane turned off by the flip that gave it `present_id`. */
    OverlayPlane(uint32_t layer, uint64_t present_id);
    /* A plane showing `allocation`, at segment address `address`. */
    OverlayPlane(uint32_t layer, uint64_t present_id, void *allocation, int64_t address,
                 const DxgkMultiplaneOverlayAttributes3Record &attributes);

    uint32_t          layer() const;
    uint64_t          present_id() const;
    /* Whether a flip ever named the layer: one that did is reported at every
     * vsync, shown or not. */
    bool              flipped() const;
    bool              shown() const;
    void             *allocation() const;
    int64_t           address() const;
    const RectRecord &source() const;
    const RectRecord &destination() const;
    const RectRecord &clip() const;
    /* DXGK_MULTIPLANE_OVERLAY_FLAGS: the vertical and horizontal flips. */
    uint32_t          flips() const;
    /* Whether the plane's alpha blends it over the planes beneath. */
    bool              blended() const;
    /* Whether it is filtered when scaled, rather than sampled at the nearest. */
    bool              filtered() const;
    /* D3DDDI_COLOR_SPACE_TYPE: how its values are colours. */
    uint32_t          colour_space() const;

private:
    uint32_t   layer_;
    uint64_t   present_id_;
    void      *allocation_;
    int64_t    address_;
    RectRecord source_;
    RectRecord destination_;
    RectRecord clip_;
    uint32_t   flips_;
    bool       blended_;
    bool       filtered_;
    uint32_t   colour_space_;
};

}  // namespace av

#endif  // AV_OVERLAY_PLANE_HPP
