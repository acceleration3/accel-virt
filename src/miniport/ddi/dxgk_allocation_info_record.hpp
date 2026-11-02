#ifndef AV_DXGK_ALLOCATION_INFO_RECORD_HPP
#define AV_DXGK_ALLOCATION_INFO_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkAllocationInfoRecord {
    void     *pPrivateDriverData;
    uint32_t PrivateDriverDataSize;
    uint32_t Alignment;
    uint64_t Size;
    uint64_t PitchAlignedSize;
    uint32_t HintedBank;
    uint32_t PreferredSegment;
    uint32_t SupportedReadSegmentSet;
    uint32_t SupportedWriteSegmentSet;
    uint32_t EvictionSegmentSet;
    uint32_t MaximumRenamingListLength;
    void     *hAllocation;
    uint32_t Flags;
    uint32_t FlagsPadding;
    void     *pAllocationUsageHint;
    uint32_t AllocationPriority;
    uint32_t Flags2;
};

}

#endif
