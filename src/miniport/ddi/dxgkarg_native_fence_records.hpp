#ifndef AV_DXGKARG_NATIVE_FENCE_RECORDS_HPP
#define AV_DXGKARG_NATIVE_FENCE_RECORDS_HPP

#include <stdint.h>

namespace av {
enum : uint32_t { native_fence_private_data_bytes = 64u };

enum : uint32_t {
    native_fence_type_default = 0u,
    native_fence_type_intra_gpu = 1u,
};

struct DxgkargCreateNativeFenceRecord {
    void    *hGlobalNativeFence;
    uint32_t Type;
    uint32_t TypePadding;
    uint64_t CurrentValueSystemProcessGpuVa;
    uint64_t MonitoredValueSystemProcessGpuVa;
    uint8_t  pPrivateDriverData[native_fence_private_data_bytes];
    uint32_t Flags;
    uint8_t  Reserved[32];
};

struct DxgkargDestroyNativeFenceRecord {
    void    *hGlobalNativeFence;
    uint32_t Flags;
    uint8_t  Reserved[32];
};

struct DxgkargUpdateNativeFenceValuesRecord {
    void   **NativeFenceArray;
    uint64_t *UpdatedValueArray;
    void   **ValueKernelCpuVa;
    uint32_t NumFences;
    uint32_t Flags;
    uint8_t  Reserved[28];
};

struct DxgkNativeFenceCapsRecord {
    uint32_t MonitoredValueStride;
    uint8_t  MapToGpuSystemProcess;
    uint8_t  MapToGpuSystemProcessPadding[3];
    uint64_t MinimumAddress;
    uint64_t MaximumAddress;
    uint8_t  Reserved[28];
};

}

#endif
