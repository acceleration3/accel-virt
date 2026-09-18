#include "core/adapter/driver_capabilities.hpp"

#include "core/ddi/dxgkarg_doorbell_records.hpp"
#include "core/ddi/dxgkarg_native_fence_records.hpp"

#include "core/ddi/dxgk_driver_caps_record.hpp"
#include "core/ddi/dxgk_physical_adapter_caps_record.hpp"
#include "core/ddi/dxgk_wddm_device_caps_record.hpp"
#include "core/ddi/dxgkarg_control_mode_behavior_record.hpp"
#include "core/display/hardware_cursor.hpp"
#include "core/miniport/adapter_info_query.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

DriverCapabilities::DriverCapabilities(Platform &platform)
    : platform_(platform), wddm_version_(wddm_3_2), highest_acceptable_address_(any_address),
      mode_behaviors_(0u), display_extension_(0u)
      {
          AV_SCOPE("DriverCapabilities::DriverCapabilities");
      }

Status DriverCapabilities::report_driver_caps(const AdapterInfoQuery &query) const
{
    AV_SCOPE("DriverCapabilities::report_driver_caps");
    platform_.require_irql_at_most(Irql::passive, "DriverCapabilities::report_driver_caps");
    DxgkDriverCapsRecord caps = {};
    caps.HighestAcceptableAddress = highest_acceptable_address_;
    caps.WDDMVersion = wddm_version_;
    caps.SchedulingCaps = scheduling_multi_engine_aware | scheduling_native_gpu_fence;
    caps.MemoryManagementCaps = memory_virtual_addressing | memory_gpu_mmu;
    caps.GpuEngineTopologyNbAsymetricProcessingNodes = node_count;
    caps.FlipCaps = flip_on_vsync_mmio;
    caps.SupportPerEngineTDR = 1u;
    caps.MaxPointerWidth = HardwareCursor::max_side;
    caps.MaxPointerHeight = HardwareCursor::max_side;
    caps.PointerCaps = pointer_caps;
    return query.write_output(&caps, sizeof caps);
}

Status DriverCapabilities::report_wddm_device_caps(const AdapterInfoQuery &query) const
{
    AV_SCOPE("DriverCapabilities::report_wddm_device_caps");
    platform_.require_irql_at_most(Irql::passive, "DriverCapabilities::report_wddm_device_caps");
    const Status input = query.require_input(interface_version_bytes);
    if (input != Status::ok) return input;
    DxgkWddmDeviceCapsRecord caps = {};
    caps.WDDMVersion = wddm_version_;
    return query.write_output(&caps, sizeof caps);
}

Status DriverCapabilities::report_user_mode_submission_caps(const AdapterInfoQuery &query) const
{
    AV_SCOPE("DriverCapabilities::report_user_mode_submission_caps");
    platform_.require_irql_at_most(Irql::passive, "DriverCapabilities::report_user_mode_submission_caps");
    DxgkUserModeSubmissionCapsRecord caps = {};
    caps.DoorbellSizeInBytes = doorbell_store_bytes;
    return query.write_output(&caps, sizeof caps);
}

Status DriverCapabilities::report_native_fence_caps(const AdapterInfoQuery &query) const
{
    AV_SCOPE("DriverCapabilities::report_native_fence_caps");
    platform_.require_irql_at_most(Irql::passive, "DriverCapabilities::report_native_fence_caps");
    DxgkNativeFenceCapsRecord caps = {};
    /* One monitored value to a fence: this driver packs none of them together,
     * so the OS may place each where it likes. */
    caps.MonitoredValueStride = sizeof(uint64_t);
    caps.MapToGpuSystemProcess = 0u;
    return query.write_output(&caps, sizeof caps);
}

Status DriverCapabilities::report_physical_adapter_caps(const AdapterInfoQuery &query, void *device_handle) const
{
    AV_SCOPE("DriverCapabilities::report_physical_adapter_caps");
    platform_.require_irql_at_most(Irql::passive, "DriverCapabilities::report_physical_adapter_caps");
    uint32_t physical_adapter = 0u;
    Status   status = query.read_input(&physical_adapter, sizeof physical_adapter);
    if (status == Status::ok && physical_adapter != 0u) status = Status::invalid_parameter;
    if (status != Status::ok) return status;
    DxgkPhysicalAdapterCapsRecord caps = {};
    caps.NumExecutionNodes = static_cast<uint16_t>(node_count);
    caps.PagingNodeIndex = 0u;
    caps.DxgkPhysicalAdapterHandle = device_handle;
    caps.Flags = physical_gpu_mmu;
    return query.write_output(&caps, sizeof caps);
}

Status DriverCapabilities::report_display_caps_extension(const AdapterInfoQuery &query) const
{
    AV_SCOPE("DriverCapabilities::report_display_caps_extension");
    platform_.require_irql_at_most(Irql::passive, "DriverCapabilities::report_display_caps_extension");
    return query.write_output(&display_extension_, sizeof display_extension_);
}

void DriverCapabilities::answer_mode_behavior(DxgkargControlModeBehaviorRecord &request) const
{
    AV_SCOPE("DriverCapabilities::answer_mode_behavior");
    platform_.require_irql_at_most(Irql::passive, "DriverCapabilities::answer_mode_behavior");
    request.Satisfied = request.Request & mode_behaviors_;
    request.NotSatisfied = request.Request & ~mode_behaviors_;
}

}  // namespace av
