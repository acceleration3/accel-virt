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

}

#endif
