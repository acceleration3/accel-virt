#ifndef AV_DXGK_MULTIPLANE_OVERLAY_ATTRIBUTES3_RECORD_HPP
#define AV_DXGK_MULTIPLANE_OVERLAY_ATTRIBUTES3_RECORD_HPP

#include "miniport/ddi/rect_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkMultiplaneOverlayAttributes3Record {
    uint32_t       Flags;
    RectRecord SrcRect;
    RectRecord DstRect;
    RectRecord ClipRect;
    uint32_t       Rotation;
    uint32_t       Blend;
    uint32_t       ColorSpaceType;
    uint32_t       StretchQuality;
    uint32_t       SDRWhiteLevel;
    uint32_t       DirtyRectCnt;
    const RectRecord *pDirtyRects;
};

constexpr uint32_t overlay_vertical_flip = 0x1u;
constexpr uint32_t overlay_horizontal_flip = 0x2u;
constexpr uint32_t overlay_blend_alpha = 0x1u;
constexpr uint32_t rotation_identity = 1u;

}

#endif
