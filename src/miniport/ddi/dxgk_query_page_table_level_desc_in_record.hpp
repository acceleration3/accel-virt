/* DXGK_QUERYPAGETABLELEVELDESCIN, the input of DXGKQAITYPE_PAGETABLELEVELDESC. A
 * DDI record; src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGK_QUERY_PAGE_TABLE_LEVEL_DESC_IN_RECORD_HPP
#define AV_DXGK_QUERY_PAGE_TABLE_LEVEL_DESC_IN_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkQueryPageTableLevelDescInRecord {
    uint16_t LevelIndex;
    uint16_t PhysicalAdapterIndex;
};

}  // namespace av

#endif  // AV_DXGK_QUERY_PAGE_TABLE_LEVEL_DESC_IN_RECORD_HPP
