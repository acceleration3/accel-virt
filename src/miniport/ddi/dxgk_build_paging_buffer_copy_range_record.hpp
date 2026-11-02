#ifndef AV_DXGK_BUILD_PAGING_BUFFER_COPY_RANGE_RECORD_HPP
#define AV_DXGK_BUILD_PAGING_BUFFER_COPY_RANGE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkBuildPagingBufferCopyRangeRecord {
    uint32_t NumPageTableEntries;
    uint32_t NumPageTableEntriesPadding;
    uint64_t SrcPageTableAddress;
    uint64_t DstPageTableAddress;
    uint32_t SrcStartPteIndex;
    uint32_t DstStartPteIndex;
};

}

#endif
