/* One device's open of an allocation. Its address is the
 * hDeviceSpecificAllocation dxgkrnl passes back to CloseAllocation. */
#ifndef AV_DEVICE_ALLOCATION_HPP
#define AV_DEVICE_ALLOCATION_HPP

#include <stdint.h>

namespace av {

class Allocation;
class Device;
class SharedObject;

class DeviceAllocation {
public:
    /* `shared` is the object this open counted, or null for an allocation
     * that is not shared: a close has to uncount exactly what its open
     * counted, and the allocation alone cannot say (feature 053). */
    DeviceAllocation(Device &device, Allocation &allocation, SharedObject *shared);
    DeviceAllocation(const DeviceAllocation &) = delete;
    DeviceAllocation &operator=(const DeviceAllocation &) = delete;

    Device       &device() const;
    Allocation   &allocation() const;
    SharedObject *shared() const;
    /* The point the device named for its next present from this allocation
     * (NextPointRecord), 0 when none was named; forgotten once that present's
     * last pass has moved. */
    void          name_point(uint32_t point);
    uint32_t      point() const;
    void          forget_point();

private:
    Device       &device_;
    Allocation   &allocation_;
    SharedObject *shared_;
    uint32_t      point_;
};

}  // namespace av

#endif  // AV_DEVICE_ALLOCATION_HPP
