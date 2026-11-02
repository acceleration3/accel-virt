#ifndef AV_DXGKARG_PRESENT_RECORD_HPP
#define AV_DXGKARG_PRESENT_RECORD_HPP

#include "miniport/ddi/d3dddi_patch_location_list_record.hpp"
#include "miniport/ddi/rect_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkargPresentRecord {
    void                          *pDmaBuffer;
    uint32_t                      DmaSize;
    uint32_t                      DmaSizePadding;
    void                          *pDmaBufferPrivateData;
    uint32_t                      DmaBufferPrivateDataSize;
    uint32_t                      DmaBufferPrivateDataSizePadding;
    const void                    *pAllocationList;
    D3dddiPatchLocationListRecord *pPatchLocationListOut;
    uint32_t                      PatchLocationListOutSize;
    uint32_t                      MultipassOffset;
    uint32_t                      Color;
    RectRecord                    DstRect;
    RectRecord                    SrcRect;
    uint32_t                      SubRectCnt;
    const RectRecord              *pDstSubRects;
    int32_t                       FlipInterval;
    uint32_t                      Flags;
    uint32_t                      DmaBufferSegmentId;
    uint32_t                      DmaBufferSegmentIdPadding;
    int64_t                       DmaBufferPhysicalAddress;
    uint32_t                      Reserved;
    uint32_t                      ReservedPadding;
    uint64_t                      DmaBufferGpuVirtualAddress;
    uint32_t                      NumSrcAllocations;
    uint32_t                      NumDstAllocations;
    uint32_t                      PrivateDriverDataSize;
    uint32_t                      PrivateDriverDataSizePadding;
    void                          *pPrivateDriverData;
};

}

#endif
