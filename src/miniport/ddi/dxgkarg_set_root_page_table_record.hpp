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

}

#endif
