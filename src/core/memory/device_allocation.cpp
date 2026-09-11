#include "core/memory/device_allocation.hpp"

namespace av {

DeviceAllocation::DeviceAllocation(Device &device, Allocation &allocation) : device_(device), allocation_(allocation) {}

Device     &DeviceAllocation::device() const { return device_; }
Allocation &DeviceAllocation::allocation() const { return allocation_; }

}  // namespace av
