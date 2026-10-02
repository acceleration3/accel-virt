/* DXGK_GPUMMUCAPS, as DXGKQAITYPE_GPUMMUCAPS answers it. A DDI record: Flags is
 * the capability bitfield's UINT and LegacyBehaviors its struct's;
 * src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGK_GPU_MMU_CAPS_RECORD_HPP
#define AV_DXGK_GPU_MMU_CAPS_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkGpuMmuCapsRecord {
    uint32_t Flags;
    int32_t  PageTableUpdateMode;
    uint32_t VirtualAddressBitCount;
    uint32_t LeafPageTableSizeFor64KPagesInBytes;
    uint32_t PageTableLevelCount;
    uint32_t LegacyBehaviors;
};

}  // namespace av

#endif  // AV_DXGK_GPU_MMU_CAPS_RECORD_HPP
