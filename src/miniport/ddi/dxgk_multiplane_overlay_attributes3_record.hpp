/* DXGK_MULTIPLANE_OVERLAY_ATTRIBUTES3: what a plane shows and where. A DDI
 * record; src/km/ddi_record_layout.cpp proves each offset and the size. */
#ifndef AV_DXGK_MULTIPLANE_OVERLAY_ATTRIBUTES3_RECORD_HPP
#define AV_DXGK_MULTIPLANE_OVERLAY_ATTRIBUTES3_RECORD_HPP

#include "miniport/ddi/rect_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkMultiplaneOverlayAttributes3Record {
    uint32_t       Flags;            /* DXGK_MULTIPLANE_OVERLAY_FLAGS */
    RectRecord SrcRect;
    RectRecord DstRect;
    RectRecord ClipRect;
    uint32_t       Rotation;         /* D3DDDI_ROTATION */
    uint32_t       Blend;            /* DXGK_MULTIPLANE_OVERLAY_BLEND */
    uint32_t       ColorSpaceType;   /* D3DDDI_COLOR_SPACE_TYPE */
    uint32_t       StretchQuality;   /* DXGK_MULTIPLANE_OVERLAY_STRETCH_QUALITY */
    uint32_t       SDRWhiteLevel;
    uint32_t       DirtyRectCnt;
    const RectRecord *pDirtyRects;
};

/* DXGK_MULTIPLANE_OVERLAY_FLAGS, DXGK_MULTIPLANE_OVERLAY_BLEND and D3DDDI_ROTATION. */
constexpr uint32_t overlay_vertical_flip = 0x1u;
constexpr uint32_t overlay_horizontal_flip = 0x2u;
constexpr uint32_t overlay_blend_alpha = 0x1u;
constexpr uint32_t rotation_identity = 1u;

}  // namespace av

#endif  // AV_DXGK_MULTIPLANE_OVERLAY_ATTRIBUTES3_RECORD_HPP
