#ifndef AV_DXGK_SEGMENT_DESCRIPTOR4_RECORD_HPP
#define AV_DXGK_SEGMENT_DESCRIPTOR4_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkSegmentDescriptor4Record {
    uint32_t Flags;
    uint32_t FlagsPadding;
    int64_t  BaseAddress;
    uint64_t Size;
    uint64_t CommitLimit;
    uint64_t SystemMemoryEndAddress;
    int64_t  CpuTranslatedAddress;
    uint64_t CpuHostApertureSizeInPages;
    uint32_t NumInvalidMemoryRanges;
    uint32_t NumInvalidMemoryRangesPadding;
    uint64_t VprRangeStartOffset;
    uint64_t VprRangeSize;
    uint32_t VprAlignment;
    uint32_t NumVprSupported;
    uint32_t VprReserveSize;
    uint32_t NumUEFIFrameBufferRanges;
};

}

#endif
