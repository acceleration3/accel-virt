/* DXGKARG_BUILDPAGINGBUFFER, as DxgkDdiBuildPagingBuffer receives and answers
 * it. A DDI record: Reserved is the operation union, read only through its
 * Reserved member. src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_BUILD_PAGING_BUFFER_RECORD_HPP
#define AV_DXGKARG_BUILD_PAGING_BUFFER_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargBuildPagingBufferRecord {
    void     *pDmaBuffer;
    uint32_t DmaSize;
    uint32_t DmaSizePadding;
    void     *pDmaBufferPrivateData;
    uint32_t DmaBufferPrivateDataSize;
    int32_t  Operation;
    uint32_t MultipassOffset;
    uint32_t MultipassOffsetPadding;
    uint32_t Reserved[64];
    void     *hSystemContext;
    uint64_t DmaBufferGpuVirtualAddress;
    uint32_t DmaBufferWriteOffset;
    uint32_t Padding;
};

}  // namespace av

#endif  // AV_DXGKARG_BUILD_PAGING_BUFFER_RECORD_HPP
