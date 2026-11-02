#include "miniport/display/monitor_modes.hpp"

#include "miniport/ddi/dxgkarg_recommend_monitor_modes_record.hpp"
#include "miniport/display/display_mode_list.hpp"
#include "miniport/display/display_sources.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

MonitorModes::MonitorModes(Platform &platform, const DisplayModeList &modes, const DisplaySources &sources,
                           const DxgkargRecommendMonitorModesRecord &request)
    : platform_(platform), sources_(sources), modes_(modes), request_(request)
    {
        AV_SCOPE("MonitorModes::MonitorModes");
    }

NtStatus MonitorModes::recommend()
{
    AV_SCOPE("MonitorModes::recommend");
    platform_.require_irql_at_most(Irql::passive, "MonitorModes::recommend");
    if (request_.VideoPresentTargetId >= sources_.count() || request_.hMonitorSourceModeSet == nullptr ||
        request_.pMonitorSourceModeSetInterface == nullptr)
        return NtStatus(Status::invalid_parameter);
    Status status = Status::ok;
    for (uint32_t index = 0u; status == Status::ok && index < modes_.count(); index++) status = add(modes_.mode(index));
    return NtStatus(status);
}

Status MonitorModes::add(const DisplayMode &mode)
{
    AV_SCOPE("MonitorModes::add");
    const DxgkMonitorSourceModeSetInterfaceRecord &set_interface = *request_.pMonitorSourceModeSetInterface;
    D3dkmdtMonitorSourceModeRecord                *info = nullptr;
    const Status status = NtStatus(set_interface.pfnCreateNewModeInfo(request_.hMonitorSourceModeSet, &info)).status();
    if (status != Status::ok) return status;
    if (info == nullptr) return Status::malformed;
    mode.describe(info->VideoSignalInfo);
    info->ColorBasis = color_basis_srgb;
    info->ColorCoeffDynamicRanges[0] = bits_per_colour_channel;
    info->ColorCoeffDynamicRanges[1] = bits_per_colour_channel;
    info->ColorCoeffDynamicRanges[2] = bits_per_colour_channel;
    info->ColorCoeffDynamicRanges[3] = 0u;
    info->Origin = origin_driver;
    info->Preference = modes_.is_preferred(mode) ? preferred : not_preferred;
    const NtStatus added(set_interface.pfnAddMode(request_.hMonitorSourceModeSet, info));
    if (added.succeeded()) return Status::ok;
    const Status released = NtStatus(set_interface.pfnReleaseModeInfo(request_.hMonitorSourceModeSet, info)).status();
    if (static_cast<uint32_t>(added.value()) == NtStatus::mode_already_in_mode_set) return released;
    return added.status();
}

}
