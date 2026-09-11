/* DXGKARG_CANCELCOMMAND, as DxgkDdiCancelCommand receives it. A DDI record;
 * src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_CANCEL_COMMAND_RECORD_HPP
#define AV_DXGKARG_CANCEL_COMMAND_RECORD_HPP

#include "d3dddi_patch_location_list_record.hpp"
#include "dxgk_allocation_list_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkargCancelCommandRecord {
    void                                *hContext;
    void                                *pDmaBuffer;
    uint32_t                            DmaBufferSize;
    uint32_t                            DmaBufferSubmissionStartOffset;
    uint32_t                            DmaBufferSubmissionEndOffset;
    uint32_t                            DmaBufferSubmissionEndOffsetPadding;
    void                                *pDmaBufferPrivateData;
    uint32_t                            DmaBufferPrivateDataSize;
    uint32_t                            DmaBufferPrivateDataSubmissionStartOffset;
    uint32_t                            DmaBufferPrivateDataSubmissionEndOffset;
    uint32_t                            DmaBufferPrivateDataSubmissionEndOffsetPadding;
    const DxgkAllocationListRecord      *pAllocationList;
    uint32_t                            AllocationListSize;
    uint32_t                            AllocationListSizePadding;
    const D3dddiPatchLocationListRecord *pPatchLocationList;
    uint32_t                            PatchLocationListSize;
    uint32_t                            PatchLocationListSubmissionStart;
    uint32_t                            PatchLocationListSubmissionLength;
    uint32_t                            PatchLocationListSubmissionLengthPadding;
    uint64_t                            DmaBufferVirtualAddress;
    uint32_t                            DmaBufferUmdPrivateDataSize;
    uint32_t                            DmaBufferUmdPrivateDataSizePadding;
};

}  // namespace av

#endif  // AV_DXGKARG_CANCEL_COMMAND_RECORD_HPP
