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

}

#endif
