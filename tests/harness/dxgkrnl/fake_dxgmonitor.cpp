#include "harness/dxgkrnl/fake_dxgmonitor.hpp"

#include "core/ddi/dxgkarg_update_monitor_link_info_record.hpp"

namespace av {

FakeDxgmonitor::FakeDxgmonitor(MockPlatform &platform, const DdiTable &table, void *adapter,
                               std::uint32_t driver_version)
    : platform_(platform), table_(table), adapter_(adapter), driver_version_(driver_version), usage_hints_(0u),
      capabilities_(0u), dithering_support_(0u) {}

std::int32_t FakeDxgmonitor::update_link_info(std::uint32_t target, std::uint32_t usage)
{
    using UpdateMonitorLinkInfo = std::int32_t (*)(void *, DxgkargUpdateMonitorLinkInfoRecord *);
    const auto routine = reinterpret_cast<UpdateMonitorLinkInfo>(table_.routine(DdiSlot::DxgkDdiUpdateMonitorLinkInfo));
    DxgkargUpdateMonitorLinkInfoRecord request = {};
    request.VideoPresentTargetId = target;
    request.MonitorLinkInfoUsageHints = (usage != 0u ? hint_hidden : 0u) | (usage == 1u ? hint_head_mounted : 0u);
    platform_.set_irql(Irql::passive);
    std::int32_t status = routine != nullptr ? routine(adapter_, &request) : status_not_supported;
    usage_hints_ = request.MonitorLinkInfoUsageHints;
    if (status >= 0) status = filter(request.MonitorLinkInfoCapabilities, request.MonitorLinkInfoDitheringSupport);
    if (status < 0) {
        capabilities_ = 0u;
        dithering_support_ = 0u;
    }
    return status < 0 && driver_version_ < driver_2_4 ? 0 : status;
}

std::int32_t FakeDxgmonitor::filter(std::uint32_t capabilities, std::uint32_t dithering)
{
    const std::uint32_t allowed = caps_before_2_4 | (driver_version_ >= driver_2_6 ? caps_variable_refresh : 0u);
    const std::uint32_t claimed = capabilities & ~caps_target_independent_primary;
    if ((claimed & ~allowed) != 0u && driver_version_ >= driver_2_4) return status_unsuccessful;
    capabilities_ = claimed & allowed;
    dithering_support_ = (dithering & dithering_preference) != 0u ? 0u : dithering;
    return 0;
}

std::uint32_t FakeDxgmonitor::usage_hints() const { return usage_hints_; }

std::uint32_t FakeDxgmonitor::capabilities() const { return capabilities_; }

std::uint32_t FakeDxgmonitor::dithering_support() const { return dithering_support_; }

}  // namespace av
