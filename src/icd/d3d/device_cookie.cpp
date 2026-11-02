#include "icd/d3d/device_cookie.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

DeviceCookie::DeviceCookie() : magic_(0ull), device_(nullptr)
{
    AV_SCOPE("DeviceCookie::DeviceCookie");
}

void DeviceCookie::write(UmdDevice &device)
{
    AV_SCOPE("DeviceCookie::write");
    device_ = &device;
    magic_ = magic;
}

UmdDevice *DeviceCookie::named() const
{
    AV_SCOPE("DeviceCookie::named");
    if (magic_ != magic) return nullptr;
    return device_;
}

void DeviceCookie::clear()
{
    AV_SCOPE("DeviceCookie::clear");
    magic_ = 0ull;
    device_ = nullptr;
}

bool DeviceCookie::invariants_hold() const
{
    AV_SCOPE("DeviceCookie::invariants_hold");
    if (magic_ == magic) return device_ != nullptr;
    return magic_ == 0ull ? device_ == nullptr : true;
}

}
