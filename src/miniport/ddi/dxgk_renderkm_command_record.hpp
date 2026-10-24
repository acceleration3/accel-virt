/* DXGK_RENDERKM_COMMAND and the one argument block of its union that is read,
 * as CDD's command buffer carries them to DxgkDdiRenderKm (d3dkmddi.h:490-629).
 * DDI records; src/km/ddi_record_layout.cpp proves each offset.
 *
 * Together because the command is a header over its arguments, and the
 * remaining five argument blocks join it when something reads them.
 * DxgkRenderkmCommandRecord is therefore not as large as the WDK's: a command
 * is walked by its CommandSize, never by sizeof. */
#ifndef AV_DXGK_RENDERKM_COMMAND_RECORD_HPP
#define AV_DXGK_RENDERKM_COMMAND_RECORD_HPP

#include "miniport/ddi/rect_record.hpp"

#include <stdint.h>

namespace av {

/* DXGK_RENDERKM_OPERATION */
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

}  // namespace av

#endif  // AV_DXGK_RENDERKM_COMMAND_RECORD_HPP
