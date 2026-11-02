#ifndef AV_DXGKARG_MULTIPLANE_OVERLAY_CAPS_RECORDS_HPP
#define AV_DXGKARG_MULTIPLANE_OVERLAY_CAPS_RECORDS_HPP

#include <stdint.h>

namespace av {

struct DxgkargGetMultiPlaneOverlayCapsRecord {
    uint32_t VidPnSourceId;
    uint32_t MaxPlanes;
    uint32_t MaxRGBPlanes;
    uint32_t MaxYUVPlanes;
    uint32_t OverlayCaps;
    uint32_t MaxStretchFactor;
    uint32_t MaxShrinkFactor;
};

struct DxgkargGetPostCompositionCapsRecord {
    uint32_t VidPnSourceId;
    uint32_t MaxStretchFactor;
    uint32_t MaxShrinkFactor;
};

constexpr uint32_t overlay_caps_vertical_flip = 0x4u;
constexpr uint32_t overlay_caps_horizontal_flip = 0x8u;
constexpr uint32_t overlay_caps_stretch_rgb = 0x10u;
constexpr uint32_t overlay_caps_stretch_yuv = 0x20u;
constexpr uint32_t overlay_caps_bilinear_filter = 0x40u;
constexpr uint32_t overlay_caps_shared = 0x100u;

}

#endif
