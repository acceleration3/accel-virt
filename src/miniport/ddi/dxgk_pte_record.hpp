/* DXGK_PTE, one page table entry as VidMm describes it. A DDI record: Flags is
 * the bitfield's ULONGLONG and PageAddress the address union (the high 52 bits
 * of the page's address); src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGK_PTE_RECORD_HPP
#define AV_DXGK_PTE_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkPteRecord {
    uint64_t Flags;
    uint64_t PageAddress;
};

}  // namespace av

#endif  // AV_DXGK_PTE_RECORD_HPP
