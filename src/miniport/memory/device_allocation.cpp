#include "miniport/memory/device_allocation.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

DeviceAllocation::DeviceAllocation(Device &device, Allocation &allocation, SharedObject *shared)
    : device_(device), allocation_(allocation), shared_(shared), point_(0u)
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

void DeviceAllocation::name_point(uint32_t point)
{
    AV_SCOPE("DeviceAllocation::name_point");
    point_ = point;
}

uint32_t DeviceAllocation::point() const
{
    AV_SCOPE("DeviceAllocation::point");
    return point_;
}

void DeviceAllocation::forget_point()
{
    AV_SCOPE("DeviceAllocation::forget_point");
    point_ = 0u;
}

}  // namespace av
