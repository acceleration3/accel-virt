/* DXGK_PHYSICALADAPTERCAPS, as DXGKQAITYPE_PHYSICALADAPTERCAPS answers it. A DDI
 * record: Flags is DXGK_PHYSICALADAPTERFLAGS' UINT; src/km/ddi_record_layout.cpp
 * proves each offset. */
#ifndef AV_DXGK_PHYSICAL_ADAPTER_CAPS_RECORD_HPP
#define AV_DXGK_PHYSICAL_ADAPTER_CAPS_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkPhysicalAdapterCapsRecord {
    uint16_t NumExecutionNodes;
    uint16_t PagingNodeIndex;
    uint32_t PagingNodeIndexPadding;
    void     *DxgkPhysicalAdapterHandle;
    uint32_t Flags;
    uint32_t VPRPagingNode;
    uint32_t VirtualCopyNodeIndex;
    uint32_t VirtualCopyNodeIndexPadding;
};

}  // namespace av

#endif  // AV_DXGK_PHYSICAL_ADAPTER_CAPS_RECORD_HPP
