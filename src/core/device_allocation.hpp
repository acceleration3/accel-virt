/* One device's open of an allocation. Its address is the
 * hDeviceSpecificAllocation dxgkrnl passes back to CloseAllocation. */
#ifndef AV_DEVICE_ALLOCATION_HPP
#define AV_DEVICE_ALLOCATION_HPP

namespace av {

class Allocation;
class Device;

class DeviceAllocation {
public:
    DeviceAllocation(Device &device, Allocation &allocation);
    DeviceAllocation(const DeviceAllocation &) = delete;
    DeviceAllocation &operator=(const DeviceAllocation &) = delete;

    Device     &device() const;
    Allocation &allocation() const;

private:
    Device     &device_;
    Allocation &allocation_;
};

}  // namespace av

#endif  // AV_DEVICE_ALLOCATION_HPP
