/* Every capability the adapter reports to dxgkrnl, decided in one place.
 * Constitution VI: a capability is set here only by the feature that
 * implements it, and DXGADAPTER::Initialize's rules (tests/harness
 * FakeDxgadapter) reject a claim whose DDIs are not registered. */
#ifndef AV_DRIVER_CAPABILITIES_HPP
#define AV_DRIVER_CAPABILITIES_HPP

#include "platform.hpp"
#include "status.hpp"

#include <stdint.h>

namespace av {

class AdapterInfoQuery;
struct DxgkargControlModeBehaviorRecord;

class DriverCapabilities {
public:
    explicit DriverCapabilities(Platform &platform);
    DriverCapabilities(const DriverCapabilities &) = delete;
    DriverCapabilities &operator=(const DriverCapabilities &) = delete;

    /* DXGKQAITYPE_DRIVERCAPS: the whole DXGK_DRIVERCAPS, every unclaimed field zero. */
    [[nodiscard]] Status report_driver_caps(const AdapterInfoQuery &query) const;
    /* DXGKQAITYPE_WDDMDEVICECAPS, which dxgkrnl may ask before StartDevice. */
    [[nodiscard]] Status report_wddm_device_caps(const AdapterInfoQuery &query) const;
    /* DXGKQAITYPE_PHYSICALADAPTERCAPS for physical adapter 0, named by dxgkrnl's
     * device handle. */
    [[nodiscard]] Status report_physical_adapter_caps(const AdapterInfoQuery &query, void *device_handle) const;
    /* DxgkDdiControlModeBehavior: each requested behavior satisfied only if
     * this driver implements it. */
    void                 answer_mode_behavior(DxgkargControlModeBehaviorRecord &request) const;

private:
    static constexpr int32_t wddm_2_1 = 0x2100;   /* DXGKDDI_WDDMv2_1 */
    enum : uint32_t {
        scheduling_multi_engine_aware = 0x1u,      /* DXGK_VIDSCHCAPS */
        memory_virtual_addressing = 0x20u,         /* DXGK_VIDMMCAPS: VidMm keys on it */
        memory_gpu_mmu = 0x40u,                    /* dxgkrnl keys on it */
        flip_on_vsync_mmio = 0x2u,                 /* DXGK_FLIPCAPS: SetVidPnSourceAddress at device IRQL */
        physical_gpu_mmu = 0x2u,                   /* DXGK_PHYSICALADAPTERFLAGS */
    };
    static constexpr uint32_t node_count = 1u;
    /* The virtio device addresses all of guest physical memory. */
    static constexpr int64_t any_address = -1;
    static constexpr uint32_t interface_version_bytes = 4u;   /* DXGK_WDDMDEVICECAPSIN */
    /* HardwareCursor draws every pointer type up to its 64x64 image. */
    static constexpr uint32_t pointer_caps = 0x7u;   /* Monochrome | Color | MaskedColor */

    Platform &platform_;
    int32_t   wddm_version_;
    int64_t   highest_acceptable_address_;
    /* DXGK_MODE_BEHAVIOR_FLAGS: neither HDR nor colorimetric control. */
    uint32_t  mode_behaviors_;
};

}  // namespace av

#endif  // AV_DRIVER_CAPABILITIES_HPP
