#ifndef AV_DXGK_BUILD_PAGING_BUFFER_UPDATE_PAGE_TABLE_RECORD_HPP
#define AV_DXGK_BUILD_PAGING_BUFFER_UPDATE_PAGE_TABLE_RECORD_HPP
#include "miniport/ddi/dxgk_pte_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkBuildPagingBufferUpdatePageTableRecord {
    uint32_t            PageTableLevel;
    uint32_t            PageTableLevelPadding;
    void                *hAllocation;
    void                *PageTableAddress;
    uint64_t            PageTableAddressRemainder;
    const DxgkPteRecord *pPageTableEntries;
    uint32_t            StartIndex;
    uint32_t            NumPageTableEntries;
    uint32_t            Reserved0;
    uint32_t            Flags;
    uint64_t            DriverProtection;
    uint64_t            AllocationOffsetInBytes;
    void                *hProcess;
    int32_t             UpdateMode;
    uint32_t            UpdateModePadding;
    const DxgkPteRecord *pPageTableEntries64KB;
    uint64_t            FirstPteVirtualAddress;
};

}

#endif
