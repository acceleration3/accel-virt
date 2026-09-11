/* DXGKARG_COLLECTDBGINFO_EXT, the optional output of DxgkDdiCollectDbgInfo. A
 * DDI record; src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_COLLECT_DBG_INFO_EXT_RECORD_HPP
#define AV_DXGKARG_COLLECT_DBG_INFO_EXT_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargCollectDbgInfoExtRecord {
    uint32_t BucketingKey;
    uint32_t CurrentDmaBufferOffset;
    uint32_t Reserved2;
    uint32_t Reserved3;
    uint32_t Reserved4;
    uint32_t Reserved5;
    uint32_t Reserved6;
    uint32_t Reserved7;
};

}  // namespace av

#endif  // AV_DXGKARG_COLLECT_DBG_INFO_EXT_RECORD_HPP
