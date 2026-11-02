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
    const uint32_t refresh = modes_.refresh_millihz();
    const DisplayModeText told(text);
    Status               found = Status::ok;
    if (!told.empty()) {
        found = told.read(modes_);
    } else {
        found = HostMonitorModes(host_).read(modes_);
        if (found != Status::ok) modes_.clear();
    }
    static_cast<void>(modes_.set_refresh(refresh));
    return found;
}

Status DisplayModes::set_refresh(uint32_t millihz)
{
    AV_SCOPE("DisplayModes::set_refresh");
    host_.require_main_loop("DisplayModes::set_refresh");
    return modes_.set_refresh(millihz);
}

const DisplayModeSet &DisplayModes::current() const
{
    AV_SCOPE("DisplayModes::current");
    return modes_;
}

}
