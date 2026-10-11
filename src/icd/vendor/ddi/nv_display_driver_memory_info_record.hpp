/* NV_DISPLAY_DRIVER_MEMORY_INFO_V3 (nvapi_lite_common.h, NVIDIA NVAPI SDK,
 * MIT): what NvAPI_GPU_GetMemoryInfo writes, sizes in KiB. Versions 1 and 2
 * are its first five and six fields; `version` says which the caller holds.
 * A DDI record. */
#ifndef AV_VENDOR_NV_DISPLAY_DRIVER_MEMORY_INFO_RECORD_HPP
#define AV_VENDOR_NV_DISPLAY_DRIVER_MEMORY_INFO_RECORD_HPP

#include <stdint.h>

namespace av::vendor {

struct NvDisplayDriverMemoryInfoRecord {
    uint32_t version;
    uint32_t dedicatedVideoMemory;
    uint32_t availableDedicatedVideoMemory;
    uint32_t systemVideoMemory;
    uint32_t sharedSystemMemory;
    uint32_t curAvailableDedicatedVideoMemory;
    uint32_t dedicatedVideoMemoryEvictionsSize;
    uint32_t dedicatedVideoMemoryEvictionCount;
};

}  // namespace av::vendor

#endif  // AV_VENDOR_NV_DISPLAY_DRIVER_MEMORY_INFO_RECORD_HPP
