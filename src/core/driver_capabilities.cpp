#include "driver_capabilities.hpp"

#include "adapter_info_query.hpp"
#include "ddi/dxgk_driver_caps_record.hpp"
#include "ddi/dxgk_wddm_device_caps_record.hpp"

namespace av {

DriverCapabilities::DriverCapabilities(Platform &platform)
    : platform_(platform), wddm_version_(wddm_1_0), highest_acceptable_address_(any_address) {}

Status DriverCapabilities::report_driver_caps(const AdapterInfoQuery &query) const
{
    platform_.require_irql_at_most(Irql::passive, "DriverCapabilities::report_driver_caps");
    DxgkDriverCapsRecord caps = {};
    caps.HighestAcceptableAddress = highest_acceptable_address_;
    caps.WDDMVersion = wddm_version_;
    return query.write_output(&caps, sizeof caps);
}

Status DriverCapabilities::report_wddm_device_caps(const AdapterInfoQuery &query) const
{
    platform_.require_irql_at_most(Irql::passive, "DriverCapabilities::report_wddm_device_caps");
    const Status input = query.require_input(interface_version_bytes);
    if (input != Status::ok) return input;
    DxgkWddmDeviceCapsRecord caps = {};
    caps.WDDMVersion = wddm_version_;
    return query.write_output(&caps, sizeof caps);
}

}  // namespace av
