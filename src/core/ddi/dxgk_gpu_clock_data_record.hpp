/* DXGK_GPUCLOCKDATA (DXGKARG_CALIBRATEGPUCLOCK), as DxgkDdiCalibrateGpuClock
 * fills it. A DDI record: Flags is the DXGK_GPUCLOCKDATA_FLAGS Value, and
 * d3dkmdt.h declares it under pack(1); src/km/ddi_record_layout.cpp proves each
 * offset. */
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

}  // namespace av

#endif  // AV_DXGK_GPU_CLOCK_DATA_RECORD_HPP
