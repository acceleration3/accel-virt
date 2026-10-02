#include "miniport/scheduling/device.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

Device::Device(Platform &platform, Adapter &adapter, void *runtime_handle, uint32_t flags, void *process)
    : platform_(platform), adapter_(adapter), runtime_handle_(runtime_handle), flags_(flags), process_(process)
    {
        AV_SCOPE("Device::Device");
    }

Adapter &Device::adapter() const
{
    AV_SCOPE("Device::adapter");
    platform_.require_irql_at_most(Irql::dispatch, "Device::adapter");
    return adapter_;
}

void *Device::runtime_handle() const
{
    AV_SCOPE("Device::runtime_handle");
    platform_.require_irql_at_most(Irql::dispatch, "Device::runtime_handle");
    return runtime_handle_;
}

uint32_t Device::flags() const
{
    AV_SCOPE("Device::flags");
    platform_.require_irql_at_most(Irql::dispatch, "Device::flags");
    return flags_;
}

void *Device::process() const
{
    AV_SCOPE("Device::process");
    return process_;
}

}  // namespace av
