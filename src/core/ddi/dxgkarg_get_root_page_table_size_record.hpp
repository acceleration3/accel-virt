/* DXGKARG_GETROOTPAGETABLESIZE, as DxgkDdiGetRootPageTableSize receives and answers
 * it. A DDI record; src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_GET_ROOT_PAGE_TABLE_SIZE_RECORD_HPP
#define AV_DXGKARG_GET_ROOT_PAGE_TABLE_SIZE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargGetRootPageTableSizeRecord {
    uint32_t NumberOfPte;
    uint32_t PhysicalAdapterIndex;
};

}  // namespace av

#endif  // AV_DXGKARG_GET_ROOT_PAGE_TABLE_SIZE_RECORD_HPP
