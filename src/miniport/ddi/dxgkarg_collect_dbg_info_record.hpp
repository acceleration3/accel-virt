#ifndef AV_DXGKARG_COLLECT_DBG_INFO_RECORD_HPP
#define AV_DXGKARG_COLLECT_DBG_INFO_RECORD_HPP

#include "miniport/ddi/dxgkarg_collect_dbg_info_ext_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkargCollectDbgInfoRecord {
    uint32_t                        Reason;
    uint32_t                        ReasonPadding;
    uint8_t                        *pBuffer;
    uint64_t                        BufferSize;
    DxgkargCollectDbgInfoExtRecord *pExtension;
};

}

#endif
