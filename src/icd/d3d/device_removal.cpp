#include "icd/d3d/device_removal.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/log_sink.hpp"

namespace av::d3d {
namespace {

constexpr uint32_t line_bytes = 128u;

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

Hresult DeviceRemoval::check(icd::LogSink &log, const char *entry)
{
    AV_SCOPE("DeviceRemoval::check");
    if (reason_ == hr_ok) return hr_ok;
    if (!reported_) {
        reported_ = true;
        char       line[line_bytes] = {};
        TextBuffer text(line, sizeof line);
        text.append("avumd: device removed, ");
        text.append(entry);
        text.append(" answers 0x");
        text.append_hex(static_cast<uint64_t>(static_cast<uint32_t>(reason_)), 8u);
        log.write(text.text());
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
