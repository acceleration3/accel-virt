#ifndef AV_DXGKARG_SET_POINTER_SHAPE_RECORD_HPP
#define AV_DXGKARG_SET_POINTER_SHAPE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargSetPointerShapeRecord {
    uint32_t   Flags;
    uint32_t   Width;
    uint32_t   Height;
    uint32_t   Pitch;
    uint32_t   VidPnSourceId;
    uint32_t   VidPnSourceIdPadding;
    const void *pPixels;
    uint32_t   XHot;
    uint32_t   YHot;
};

}

#endif
