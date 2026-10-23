#include "miniport/memory/device_allocation.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

DeviceAllocation::DeviceAllocation(Device &device, Allocation &allocation, SharedObject *shared)
    : device_(device), allocation_(allocation), shared_(shared), points_{}, queued_(0u), pointed_(false), starved_told_(false)
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
    pointed_ = true;
    if (queued_ == most_points) {
        for (uint32_t index = 1u; index < most_points; index++) points_[index - 1u] = points_[index];
        queued_--;
    }
    points_[queued_++] = point;
}

bool DeviceAllocation::withdraw_point(uint32_t point)
{
    AV_SCOPE("DeviceAllocation::withdraw_point");
    for (uint32_t index = queued_; index != 0u; index--) {
        if (points_[index - 1u] != point) continue;
        for (uint32_t later = index; later < queued_; later++) points_[later - 1u] = points_[later];
        queued_--;
        return true;
    }
    return false;
}

uint32_t DeviceAllocation::point() const
{
    AV_SCOPE("DeviceAllocation::point");
    return queued_ != 0u ? points_[queued_ - 1u] : 0u;
}

uint32_t DeviceAllocation::queued() const
{
    AV_SCOPE("DeviceAllocation::queued");
    return queued_;
}

void DeviceAllocation::forget_point()
{
    AV_SCOPE("DeviceAllocation::forget_point");
    queued_ = 0u;
}

bool DeviceAllocation::pointed() const
{
    AV_SCOPE("DeviceAllocation::pointed");
    return pointed_;
}

void DeviceAllocation::view(PointView &view)
{
    AV_SCOPE("DeviceAllocation::view");
    view.pointed = pointed_;
    view.queued = queued_ != 0u;
    view.point = point();
    view.first_starvation = pointed_ && queued_ == 0u && !starved_told_;
    if (view.first_starvation) starved_told_ = true;
}

}  // namespace av
