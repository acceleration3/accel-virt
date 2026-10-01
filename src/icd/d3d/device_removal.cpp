#include "icd/d3d/device_removal.hpp"

#include "core/support/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/support/log_sink.hpp"

namespace av::d3d {
namespace {


}  // namespace

DeviceRemoval::DeviceRemoval() : reason_(hr_ok), later_causes_(0u), reported_(false)
{
    AV_SCOPE("DeviceRemoval::DeviceRemoval");
}

void DeviceRemoval::remove(Hresult reason)
{
    AV_SCOPE("DeviceRemoval::remove");
    if (reason == hr_ok) return;
    if (reason_ != hr_ok) {
        later_causes_++;
        return;
    }
    reason_ = reason;
}

bool DeviceRemoval::removed() const
{
    AV_SCOPE("DeviceRemoval::removed");
    return reason_ != hr_ok;
}

Hresult DeviceRemoval::reason() const
{
    AV_SCOPE("DeviceRemoval::reason");
    return reason_;
}

Hresult DeviceRemoval::check(LogSink &log, const char *entry)
{
    AV_SCOPE("DeviceRemoval::check");
    if (reason_ == hr_ok) return hr_ok;
    if (!reported_) {
        reported_ = true;
        DriverLog lines(log, "avumd: ");
        LogLine(lines)
            .text("device removed, ")
            .text(entry)
            .text(" answers ")
            .hex(static_cast<uint32_t>(reason_), 8u)
            .write();
    }
    return reason_;
}

uint32_t DeviceRemoval::later_causes() const
{
    AV_SCOPE("DeviceRemoval::later_causes");
    return later_causes_;
}

bool DeviceRemoval::invariants_hold() const
{
    AV_SCOPE("DeviceRemoval::invariants_hold");
    if (reason_ == hr_ok && (later_causes_ != 0u || reported_)) return false;
    return true;
}

}  // namespace av::d3d
