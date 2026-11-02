#ifndef AV_DXGK_BUILD_PAGING_BUFFER_TRANSFER_VIRTUAL_RECORD_HPP
#define AV_DXGK_BUILD_PAGING_BUFFER_TRANSFER_VIRTUAL_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkBuildPagingBufferTransferVirtualRecord {
    void     *hAllocation;
    uint64_t AllocationOffsetInBytes;
    uint64_t TransferSizeInBytes;
    uint64_t SourceVirtualAddress;
    uint64_t DestinationVirtualAddress;
    uint64_t SourcePageTable;
    int32_t  TransferDirection;
    uint32_t Flags;
    uint64_t DestinationPageTable;
};

}

#endif
