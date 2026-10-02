/* DXGKARG_SETROOTPAGETABLE, as DxgkDdiSetRootPageTable receives it. A DDI record:
 * Address's D3DGPU_PHYSICAL_ADDRESS fields are prefixed with its name;
 * src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_SET_ROOT_PAGE_TABLE_RECORD_HPP
#define AV_DXGKARG_SET_ROOT_PAGE_TABLE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargSetRootPageTableRecord {
    void     *hContext;
    uint32_t AddressSegmentId;
    uint32_t AddressPadding;
    uint64_t AddressSegmentOffset;
    uint32_t NumEntries;
    uint32_t NumEntriesPadding;
};

}  // namespace av

#endif  // AV_DXGKARG_SET_ROOT_PAGE_TABLE_RECORD_HPP
