/* DXGKARG_SUBMITCOMMAND, as DxgkDdiSubmitCommand receives it. A DDI record:
 * hDevice is the union with hContext, Flags the DXGK_SUBMITCOMMANDFLAGS
 * Value. src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_SUBMIT_COMMAND_RECORD_HPP
#define AV_DXGKARG_SUBMIT_COMMAND_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargSubmitCommandRecord {
    void     *hDevice;
    uint32_t DmaBufferSegmentId;
    uint32_t DmaBufferSegmentIdPadding;
    int64_t  DmaBufferPhysicalAddress;
    uint32_t DmaBufferSize;
    uint32_t DmaBufferSubmissionStartOffset;
    uint32_t DmaBufferSubmissionEndOffset;
    uint32_t DmaBufferSubmissionEndOffsetPadding;
    void     *pDmaBufferPrivateData;
    uint32_t DmaBufferPrivateDataSize;
    uint32_t DmaBufferPrivateDataSubmissionStartOffset;
    uint32_t DmaBufferPrivateDataSubmissionEndOffset;
    uint32_t SubmissionFenceId;
    uint32_t VidPnSourceId;
    int32_t  FlipInterval;
    uint32_t Flags;
    uint32_t EngineOrdinal;
    uint64_t DmaBufferVirtualAddress;
    uint32_t NodeOrdinal;
    uint32_t Padding;
};

}  // namespace av

#endif  // AV_DXGKARG_SUBMIT_COMMAND_RECORD_HPP
