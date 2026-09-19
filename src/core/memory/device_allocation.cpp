#include "core/memory/device_allocation.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

DeviceAllocation::DeviceAllocation(Device &device, Allocation &allocation, SharedObject *shared)
    : device_(device), allocation_(allocation), shared_(shared)
{
    AV_SCOPE("DeviceAllocation::DeviceAllocation");
}

Device     &DeviceAllocation::device() const
{
    AV_SCOPE("DeviceAllocation::device");
    return device_;
}
Allocation &DeviceAllocation::allocation() const
{
    AV_SCOPE("DeviceAllocation::allocation");
    return allocation_;
}

SharedObject *DeviceAllocation::shared() const
{
    AV_SCOPE("DeviceAllocation::shared");
    return shared_;
}

}  // namespace av
