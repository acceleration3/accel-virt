#ifndef AV_DXGKARG_RENDER_RECORD_HPP
#define AV_DXGKARG_RENDER_RECORD_HPP

#include "miniport/ddi/d3dddi_patch_location_list_record.hpp"
#include "miniport/ddi/dxgk_allocation_list_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkargRenderRecord {
    const void                    *pCommand;
    uint32_t                      CommandLength;
    uint32_t                      CommandLengthPadding;
    void                          *pDmaBuffer;
    uint32_t                      DmaSize;
    uint32_t                      DmaSizePadding;
    void                          *pDmaBufferPrivateData;
    uint32_t                      DmaBufferPrivateDataSize;
    uint32_t                      DmaBufferPrivateDataSizePadding;
    DxgkAllocationListRecord      *pAllocationList;
    uint32_t                      AllocationListSize;
    uint32_t                      AllocationListSizePadding;
    D3dddiPatchLocationListRecord *pPatchLocationListIn;
    uint32_t                      PatchLocationListInSize;
    uint32_t                      PatchLocationListInSizePadding;
    D3dddiPatchLocationListRecord *pPatchLocationListOut;
    uint32_t                      PatchLocationListOutSize;
    uint32_t                      MultipassOffset;
    uint32_t                      DmaBufferSegmentId;
    uint32_t                      DmaBufferSegmentIdPadding;
    int64_t                       DmaBufferPhysicalAddress;
};

}

#endif
