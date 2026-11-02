#ifndef AV_DXGK_RENDERKM_COMMAND_RECORD_HPP
#define AV_DXGK_RENDERKM_COMMAND_RECORD_HPP

#include "miniport/ddi/rect_record.hpp"

#include <stdint.h>

namespace av {

enum class DxgkRenderkmOperation : uint32_t {
    bitblt = 1u,
    colorfill = 2u,
    alphablend = 3u,
    stretchblt = 4u,
    escape = 5u,
    transparentblt = 6u,
    cleartypeblend = 7u,
};

struct DxgkGdiargBitbltRecord {
    RectRecord SrcRect;
    RectRecord DstRect;
    uint32_t   SrcAllocationIndex;
    uint32_t   DstAllocationIndex;
    uint32_t   NumSubRects;
    uint32_t   NumSubRectsPadding;
    RectRecord *pSubRects;
    uint16_t   Rop;
    uint16_t   Rop3;
    uint32_t   SrcPitch;
    uint32_t   DstPitch;
    uint32_t   DstPitchPadding;
};

struct DxgkRenderkmCommandRecord {
    uint32_t               OpCode;
    uint32_t               CommandSize;
    DxgkGdiargBitbltRecord BitBlt;
};

}

#endif
