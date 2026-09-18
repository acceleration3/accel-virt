/* The native fence DDIs' arguments (d3dkmddi.h:3124-3346), as
 * DxgkDdiCreateNativeFence, DestroyNativeFence, UpdateMonitoredValues and
 * UpdateCurrentValuesFromCpu receive them, and the caps
 * DXGKQAITYPE_NATIVE_FENCE_CAPS answers. DDI records;
 * src/km/ddi_record_layout.cpp proves each offset.
 *
 * Four records in one header because they are one mechanism's arguments, as
 * the doorbell records are (feature 049). */
#ifndef AV_DXGKARG_NATIVE_FENCE_RECORDS_HPP
#define AV_DXGKARG_NATIVE_FENCE_RECORDS_HPP

#include <stdint.h>

namespace av {

/* D3DDDI_NATIVE_FENCE_PDD_SIZE (d3dukmdt.h:1985). */
enum : uint32_t { native_fence_private_data_bytes = 64u };

/* D3DDDI_NATIVEFENCE_TYPE: a default fence is readable and writable by both
 * sides; an intra-GPU one is the engines' own and never touched by a CPU. */
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

/* Both updates carry the same three arrays: which fences, what values, and
 * where the value the OS wants written lives. */
struct DxgkargUpdateNativeFenceValuesRecord {
    void   **NativeFenceArray;
    uint64_t *UpdatedValueArray;
    void   **ValueKernelCpuVa;
    uint32_t NumFences;
    uint32_t Flags;
    uint8_t  Reserved[28];
};

/* DXGK_NATIVE_FENCE_CAPS (d3dkmddi.h:3124): what this driver's fences need of
 * the OS. Nothing is mapped into a GPU system process here - the device reads
 * no fence memory of its own - and the monitored values are one per fence. */
struct DxgkNativeFenceCapsRecord {
    uint32_t MonitoredValueStride;
    uint8_t  MapToGpuSystemProcess;
    uint8_t  MapToGpuSystemProcessPadding[3];
    uint64_t MinimumAddress;
    uint64_t MaximumAddress;
    uint8_t  Reserved[28];
};

}  // namespace av

#endif  // AV_DXGKARG_NATIVE_FENCE_RECORDS_HPP
