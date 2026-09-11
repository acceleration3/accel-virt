#include "device.hpp"

namespace av {

Device::Device(Platform &platform, Adapter &adapter, void *runtime_handle, uint32_t flags)
    : platform_(platform), adapter_(adapter), runtime_handle_(runtime_handle), flags_(flags) {}

Adapter &Device::adapter() const
{
    platform_.require_irql_at_most(Irql::dispatch, "Device::adapter");
    return adapter_;
}

void *Device::runtime_handle() const
{
    platform_.require_irql_at_most(Irql::dispatch, "Device::runtime_handle");
    return runtime_handle_;
}

uint32_t Device::flags() const
{
    platform_.require_irql_at_most(Irql::dispatch, "Device::flags");
    return flags_;
}

}  // namespace av
