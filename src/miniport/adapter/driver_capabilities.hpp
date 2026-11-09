#ifndef AV_DRIVER_CAPABILITIES_HPP
#define AV_DRIVER_CAPABILITIES_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class AdapterInfoQuery;
class CapsetInfo;
struct DxgkargControlModeBehaviorRecord;

class DriverCapabilities {
public:
    static constexpr uint32_t doorbell_store_bytes = 4u;
    static constexpr uint32_t node_count = 2u;
    static constexpr uint32_t packet_node = 0u;
    static constexpr uint32_t hardware_queue_node = 1u;
    explicit DriverCapabilities(Platform &platform);
    void                 native_fences_allowed();
    void                 offer(const CapsetInfo &capset);
    bool                 hybrid_discrete() const;
    bool                 hybrid_integrated() const;
    DriverCapabilities(const DriverCapabilities &) = delete;
    DriverCapabilities &operator=(const DriverCapabilities &) = delete;

    [[nodiscard]] Status report_driver_caps(const AdapterInfoQuery &query) const;
    [[nodiscard]] Status report_wddm_device_caps(const AdapterInfoQuery &query) const;
    [[nodiscard]] Status report_user_mode_submission_caps(const AdapterInfoQuery &query) const;
    [[nodiscard]] Status report_native_fence_caps(const AdapterInfoQuery &query) const;
    bool                 native_fences_reportable() const;
    [[nodiscard]] Status report_physical_adapter_caps(const AdapterInfoQuery &query, void *device_handle) const;
    [[nodiscard]] Status report_display_caps_extension(const AdapterInfoQuery &query) const;
    void                 answer_mode_behavior(DxgkargControlModeBehaviorRecord &request) const;

private:
    static constexpr int32_t wddm_3_2 = 0x3200;
    enum : uint32_t {
        scheduling_multi_engine_aware = 0x1u,
        scheduling_native_gpu_fence = 0x800u,
        memory_cross_adapter_resource = 0x10u,
        memory_virtual_addressing = 0x20u,
        memory_gpu_mmu = 0x40u,
        flip_on_vsync_mmio = 0x2u,
        presentation_kernel_mode_command_buffer = 0x4u,
        presentation_gdi_limits = (2u << 10u) | (3u << 14u) | (3u << 17u),
        presentation_software_device_bitmaps = 0x10000000u,
        physical_gpu_mmu = 0x2u,
        misc_no_hybrid_discrete_dlist_dll = 0x20u,
        misc_no_hybrid_discrete_dlist_dll_mux = 0x80u,
    };
    static constexpr int64_t any_address = -1;
    static constexpr uint32_t interface_version_bytes = 4u;
    static constexpr uint32_t pointer_caps = 0x7u;

    Platform &platform_;
    bool      native_fences_;
    int32_t   wddm_version_;
    int64_t   highest_acceptable_address_;
    uint32_t  mode_behaviors_;
    uint32_t  display_extension_;
    bool      hybrid_discrete_;
    bool      hybrid_integrated_;
};

}

#endif
