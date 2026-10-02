/* DXGK_BUILDPAGINGBUFFER_FLUSHTLB, the operation union for DXGK_OPERATION_FLUSH_TLB.
 * A DDI record: RootPageTableAddress's D3DGPU_PHYSICAL_ADDRESS fields are prefixed
 * with its name; src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGK_BUILD_PAGING_BUFFER_FLUSH_TLB_RECORD_HPP
#define AV_DXGK_BUILD_PAGING_BUFFER_FLUSH_TLB_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkBuildPagingBufferFlushTlbRecord {
    uint32_t RootPageTableAddressSegmentId;
    uint32_t RootPageTableAddressPadding;
    uint64_t RootPageTableAddressSegmentOffset;
    void     *hProcess;
    uint64_t StartVirtualAddress;
    uint64_t EndVirtualAddress;
};

}  // namespace av

#endif  // AV_DXGK_BUILD_PAGING_BUFFER_FLUSH_TLB_RECORD_HPP
