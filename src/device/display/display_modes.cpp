#include "device/display/display_modes.hpp"

#include "core/trace/function_scope.hpp"
#include "device/display/display_mode_text.hpp"
#include "device/display/host_monitor_modes.hpp"
#include "device/host/host.hpp"

namespace av {

DisplayModes::DisplayModes(Host &host) : host_(host), modes_()
{
    AV_SCOPE("DisplayModes::DisplayModes");
}

Status DisplayModes::set(const char *text)
{
    AV_SCOPE("DisplayModes::set");
    host_.require_main_loop("DisplayModes::set");
    const DisplayModeText told(text);
    if (!told.empty()) return told.read(modes_);
    const Status found = HostMonitorModes(host_).read(modes_);
    if (found != Status::ok) modes_.clear();
    return found;
}

const DisplayModeSet &DisplayModes::current() const
{
    AV_SCOPE("DisplayModes::current");
    return modes_;
}

}  // namespace av
