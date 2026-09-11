/* DXGKARG_PATCH, as DxgkDdiPatch receives it. A DDI record: hDevice is the
 * union with hContext. src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_PATCH_RECORD_HPP
#define AV_DXGKARG_PATCH_RECORD_HPP

#include "core/ddi/d3dddi_patch_location_list_record.hpp"
#include "core/ddi/dxgk_allocation_list_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkargPatchRecord {
    void                                *hDevice;
    uint32_t                            DmaBufferSegmentId;
    uint32_t                            DmaBufferSegmentIdPadding;
    int64_t                             DmaBufferPhysicalAddress;
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
    uint32_t                            SubmissionFenceId;
    uint32_t                            Flags;
    uint32_t                            EngineOrdinal;
};

}  // namespace av

#endif  // AV_DXGKARG_PATCH_RECORD_HPP
