#include "icd/common/device_link.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av::icd {

DeviceLink::DeviceLink(Kmt &kmt) : device_(kmt), users_(0u), lost_(false)
{
    AV_SCOPE("DeviceLink::DeviceLink");
}

DeviceLink::~DeviceLink()
{
    AV_SCOPE("DeviceLink::~DeviceLink");
    device_.close();
}

Status DeviceLink::open(uint32_t adapter)
{
    AV_SCOPE("DeviceLink::open");
    return device_.open(adapter);
}

void DeviceLink::hold()
{
    AV_SCOPE("DeviceLink::hold");
    users_++;
}

bool DeviceLink::let_go()
{
    AV_SCOPE("DeviceLink::let_go");
    if (users_ != 0u) users_--;
    return users_ == 0u;
}

uint32_t DeviceLink::users() const
{
    AV_SCOPE("DeviceLink::users");
    return users_;
}

void DeviceLink::lose()
{
    AV_SCOPE("DeviceLink::lose");
    __atomic_store_n(&lost_, true, __ATOMIC_RELEASE);
}

bool DeviceLink::lost() const
{
    AV_SCOPE("DeviceLink::lost");
    return __atomic_load_n(&lost_, __ATOMIC_ACQUIRE);
}

GuestConnection &DeviceLink::device()
{
    AV_SCOPE("DeviceLink::device");
    return device_;
}

const GuestConnection &DeviceLink::device() const
{
    AV_SCOPE("DeviceLink::device");
    return device_;
}

void DeviceLink::report(StateWriter &writer) const
{
    AV_SCOPE("DeviceLink::report");
    writer.open("link", device_.device());
    writer.number("users", users_);
    writer.flag("lost", lost());
    writer.close();
}

bool DeviceLink::invariants_hold() const
{
    AV_SCOPE("DeviceLink::invariants_hold");
    if (!device_.invariants_hold()) return false;
    return users_ == 0u || device_.opened();
}

}  // namespace av::icd
