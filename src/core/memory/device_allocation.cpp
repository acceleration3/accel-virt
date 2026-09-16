#include "core/memory/device_allocation.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

DeviceAllocation::DeviceAllocation(Device &device, Allocation &allocation) : device_(device), allocation_(allocation)
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

}  // namespace av
