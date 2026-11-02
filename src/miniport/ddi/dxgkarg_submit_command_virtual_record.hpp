#ifndef AV_DXGKARG_SUBMIT_COMMAND_VIRTUAL_RECORD_HPP
#define AV_DXGKARG_SUBMIT_COMMAND_VIRTUAL_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargSubmitCommandVirtualRecord {
    void     *hContext;
    uint64_t DmaBufferVirtualAddress;
    uint32_t DmaBufferSize;
    uint32_t DmaBufferSizePadding;
    void     *pDmaBufferPrivateData;
    uint32_t DmaBufferPrivateDataSize;
    uint32_t DmaBufferUmdPrivateDataSize;
    uint32_t SubmissionFenceId;
    uint32_t VidPnSourceId;
    int32_t  FlipInterval;
    uint32_t Flags;
    uint32_t EngineOrdinal;
    uint32_t NodeOrdinal;
};

}

#endif
