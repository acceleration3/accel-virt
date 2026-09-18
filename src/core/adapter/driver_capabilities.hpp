/* Every capability the adapter reports to dxgkrnl, decided in one place.
 * Constitution VI: a capability is set here only by the feature that
 * implements it, and DXGADAPTER::Initialize's rules (tests/harness
 * FakeDxgadapter) reject a claim whose DDIs are not registered. */
#ifndef AV_DRIVER_CAPABILITIES_HPP
#define AV_DRIVER_CAPABILITIES_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class AdapterInfoQuery;
struct DxgkargControlModeBehaviorRecord;

class DriverCapabilities {
public:
    /* What a process stores to ring this device's doorbell: four bytes at
     * offset 0 of the page (rvk_device.h). */
    static constexpr uint32_t doorbell_store_bytes = 4u;
    /* Two nodes: node 0 is the one VidSch schedules packets on, and node 1 is
     * the hardware-scheduled one a process holds queues and doorbells on. The
     * OS checks a node's context scheduling against the node before it, so the
     * hardware-scheduled one cannot be the first (feature 049, research.md
     * §16). */
    static constexpr uint32_t node_count = 2u;
    static constexpr uint32_t packet_node = 0u;
    static constexpr uint32_t hardware_queue_node = 1u;
    explicit DriverCapabilities(Platform &platform);
    DriverCapabilities(const DriverCapabilities &) = delete;
    DriverCapabilities &operator=(const DriverCapabilities &) = delete;

    /* DXGKQAITYPE_DRIVERCAPS: the whole DXGK_DRIVERCAPS, every unclaimed field zero. */
    [[nodiscard]] Status report_driver_caps(const AdapterInfoQuery &query) const;
    /* DXGKQAITYPE_WDDMDEVICECAPS, which dxgkrnl may ask before StartDevice. */
    [[nodiscard]] Status report_wddm_device_caps(const AdapterInfoQuery &query) const;
    /* DXGKQAITYPE_USERMODESUBMISSION_CAPS (feature 049): this device's doorbell
     * is a four-byte store at offset 0 of its page, and it has no secondary
     * doorbell. Reported only because the doorbell DDIs are implemented -
     * dxgkrnl asks a driver that says yes to hand processes pages. */
    [[nodiscard]] Status report_user_mode_submission_caps(const AdapterInfoQuery &query) const;
    /* What this driver's native fences need of the OS: one monitored value per
     * fence, and no mapping into a GPU system process - the device reads no
     * fence memory of its own (feature 049). */
    [[nodiscard]] Status report_native_fence_caps(const AdapterInfoQuery &query) const;
    /* DXGKQAITYPE_PHYSICALADAPTERCAPS for physical adapter 0, named by dxgkrnl's
     * device handle. */
    [[nodiscard]] Status report_physical_adapter_caps(const AdapterInfoQuery &query, void *device_handle) const;
    /* DXGKQAITYPE_DISPLAY_DRIVERCAPS_EXTENSION: no secure display, virtual modes
     * or HDR scanout. From 2.2 dxgkrnl assumes VirtualModeSupport when this is
     * not answered. */
    [[nodiscard]] Status report_display_caps_extension(const AdapterInfoQuery &query) const;
    /* DxgkDdiControlModeBehavior: each requested behavior satisfied only if
     * this driver implements it. */
    void                 answer_mode_behavior(DxgkargControlModeBehaviorRecord &request) const;

private:
    static constexpr int32_t wddm_3_2 = 0x3200;   /* DXGKDDI_WDDMv3_2 */
    enum : uint32_t {
        scheduling_multi_engine_aware = 0x1u,      /* DXGK_VIDSCHCAPS */
        /* DXGK_VIDSCHCAPS.NativeGpuFence (d3dkmddi.h:2020): the OS checks this
         * before it allows user-mode submission (feature 049, research.md
         * §14), and this driver implements the four DDIs behind it. */
        scheduling_native_gpu_fence = 0x800u,
        memory_virtual_addressing = 0x20u,         /* DXGK_VIDMMCAPS: VidMm keys on it */
        memory_gpu_mmu = 0x40u,                    /* dxgkrnl keys on it */
        flip_on_vsync_mmio = 0x2u,                 /* DXGK_FLIPCAPS: SetVidPnSourceAddress at device IRQL */
        physical_gpu_mmu = 0x2u,                   /* DXGK_PHYSICALADAPTERFLAGS */
    };
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
    /* DXGK_DISPLAY_DRIVERCAPS_EXTENSION */
    uint32_t  display_extension_;
};

}  // namespace av

#endif  // AV_DRIVER_CAPABILITIES_HPP
