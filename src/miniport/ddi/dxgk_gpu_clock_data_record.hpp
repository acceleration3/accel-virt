#ifndef AV_DXGK_GPU_CLOCK_DATA_RECORD_HPP
#define AV_DXGK_GPU_CLOCK_DATA_RECORD_HPP

#include <stdint.h>

namespace av {

#pragma pack(push, 1)
struct DxgkGpuClockDataRecord {
    uint64_t GpuFrequency;
    uint64_t GpuClockCounter;
    uint64_t CpuClockCounter;
    uint32_t Flags;
};
#pragma pack(pop)

}

#endif
