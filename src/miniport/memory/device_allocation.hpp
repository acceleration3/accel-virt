#ifndef AV_DEVICE_ALLOCATION_HPP
#define AV_DEVICE_ALLOCATION_HPP

#include "miniport/memory/point_view.hpp"

#include <stdint.h>

namespace av {

class Allocation;
class Device;
class SharedObject;

class DeviceAllocation {
public:
    DeviceAllocation(Device &device, Allocation &allocation, SharedObject *shared);
    DeviceAllocation(const DeviceAllocation &) = delete;
    DeviceAllocation &operator=(const DeviceAllocation &) = delete;

    Device       &device() const;
    Allocation   &allocation() const;
    SharedObject *shared() const;
    static constexpr uint32_t most_points = 8u;
    void          name_point(uint32_t point);
    bool          withdraw_point(uint32_t point);
    uint32_t      point() const;
    uint32_t      queued() const;
    void          forget_point();
    bool          pointed() const;
    void          view(PointView &view);

private:
    Device       &device_;
    Allocation   &allocation_;
    SharedObject *shared_;
    uint32_t      points_[most_points];
    uint32_t      queued_;
    bool          pointed_;
    bool          starved_told_;
};

}

#endif
