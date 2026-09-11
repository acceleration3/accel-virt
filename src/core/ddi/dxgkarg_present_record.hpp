/* DXGKARG_PRESENT, as DxgkDdiPresent receives and answers it. A DDI record:
 * the allocation union is a DXGK_ALLOCATIONLIST array when a patch list comes
 * with it, and a DXGK_PRESENTALLOCATIONINFO array for a context with GPU
 * virtual addressing, which gets none; Flags is the DXGK_PRESENTFLAGS Value;
 * src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_PRESENT_RECORD_HPP
#define AV_DXGKARG_PRESENT_RECORD_HPP

#include "core/ddi/d3dddi_patch_location_list_record.hpp"
#include "core/ddi/rect_record.hpp"

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

}  // namespace av

#endif  // AV_DXGKARG_PRESENT_RECORD_HPP
