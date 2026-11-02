#ifndef AV_DXGK_QUERY_PAGE_TABLE_LEVEL_DESC_IN_RECORD_HPP
#define AV_DXGK_QUERY_PAGE_TABLE_LEVEL_DESC_IN_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkQueryPageTableLevelDescInRecord {
    uint16_t LevelIndex;
    uint16_t PhysicalAdapterIndex;
};

}

#endif
