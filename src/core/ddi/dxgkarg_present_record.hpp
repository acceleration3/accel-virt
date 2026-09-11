/* DXGKARG_PRESENT, as DxgkDdiPresent receives and answers it. A DDI record:
 * the allocation union is the DXGK_ALLOCATIONLIST pointer a driver that is not
 * WDDM 2.0 receives, Flags is the DXGK_PRESENTFLAGS Value;
 * src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_PRESENT_RECORD_HPP
#define AV_DXGKARG_PRESENT_RECORD_HPP

#include "d3dddi_patch_location_list_record.hpp"
#include "dxgk_allocation_list_record.hpp"
#include "rect_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkargPresentRecord {
    void                          *pDmaBuffer;
    uint32_t                      DmaSize;
    uint32_t                      DmaSizePadding;
    void                          *pDmaBufferPrivateData;
    uint32_t                      DmaBufferPrivateDataSize;
    uint32_t                      DmaBufferPrivateDataSizePadding;
    const DxgkAllocationListRecord *pAllocationList;
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

}  // namespace av

#endif  // AV_DXGKARG_PRESENT_RECORD_HPP
