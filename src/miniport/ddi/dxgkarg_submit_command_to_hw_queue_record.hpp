#ifndef AV_DXGKARG_SUBMIT_COMMAND_TO_HW_QUEUE_RECORD_HPP
#define AV_DXGKARG_SUBMIT_COMMAND_TO_HW_QUEUE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargSubmitCommandToHwQueueRecord {
    void     *hHwQueue;
    uint64_t HwQueueProgressFenceId;
    uint64_t DmaBufferVirtualAddress;
    uint32_t DmaBufferSize;
    uint32_t DmaBufferPrivateDataSize;
    void     *pDmaBufferPrivateData;
    uint32_t Flags;
    uint32_t FlagsPadding;
    uint64_t HwQueueProgressFenceGpuVa;
    void     *HwQueueProgressFenceCpuVa;
};

}

#endif
