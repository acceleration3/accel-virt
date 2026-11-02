#ifndef AV_DXGK_BUILD_PAGING_BUFFER_COPY_PAGE_TABLE_ENTRIES_RECORD_HPP
#define AV_DXGK_BUILD_PAGING_BUFFER_COPY_PAGE_TABLE_ENTRIES_RECORD_HPP
#include "miniport/ddi/dxgk_build_paging_buffer_copy_range_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkBuildPagingBufferCopyPageTableEntriesRecord {
    uint32_t                                   NumRanges;
    uint32_t                                   NumRangesPadding;
    const DxgkBuildPagingBufferCopyRangeRecord *pRanges;
};

}

#endif
