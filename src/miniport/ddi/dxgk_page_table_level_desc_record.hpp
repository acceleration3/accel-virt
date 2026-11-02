#ifndef AV_DXGK_PAGE_TABLE_LEVEL_DESC_RECORD_HPP
#define AV_DXGK_PAGE_TABLE_LEVEL_DESC_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkPageTableLevelDescRecord {
    uint32_t PageTableIndexBitCount;
    uint32_t PageTableSegmentId;
    uint32_t PagingProcessPageTableSegmentId;
    uint32_t PageTableSizeInBytes;
    uint32_t PageTableAlignmentInBytes;
};

}

#endif
