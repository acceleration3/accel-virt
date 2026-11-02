#ifndef AV_DXGK_BUILD_PAGING_BUFFER_FILL_VIRTUAL_RECORD_HPP
#define AV_DXGK_BUILD_PAGING_BUFFER_FILL_VIRTUAL_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkBuildPagingBufferFillVirtualRecord {
    void     *hAllocation;
    uint64_t AllocationOffsetInBytes;
    uint64_t FillSizeInBytes;
    uint32_t FillPattern;
    uint32_t FillPatternPadding;
    uint64_t DestinationVirtualAddress;
};

}

#endif
