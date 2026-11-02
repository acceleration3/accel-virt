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

}

#endif
