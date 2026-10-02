/* DXGKARG_CREATEHWQUEUE, as DxgkDdiCreateHwQueue receives and answers it. A DDI
 * record; src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_CREATE_HW_QUEUE_RECORD_HPP
#define AV_DXGKARG_CREATE_HW_QUEUE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargCreateHwQueueRecord {
    void     *hHwQueue;
    uint32_t Flags;
    uint32_t PrivateDriverDataSize;
    void     *pPrivateDriverData;
    uint32_t hHwQueueProgressFence;
    uint32_t hHwQueueProgressFencePadding;
    void     *HwQueueProgressFenceCPUVirtualAddress;
    uint64_t HwQueueProgressFenceGPUVirtualAddress;
};

}  // namespace av

#endif  // AV_DXGKARG_CREATE_HW_QUEUE_RECORD_HPP
