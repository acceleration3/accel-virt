/* One device's open of an allocation. Its address is the
 * hDeviceSpecificAllocation dxgkrnl passes back to CloseAllocation. */
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
    /* `shared` is the object this open counted, or null for an allocation
     * that is not shared: a close has to uncount exactly what its open
     * counted, and the allocation alone cannot say (feature 053). */
    DeviceAllocation(Device &device, Allocation &allocation, SharedObject *shared);
    DeviceAllocation(const DeviceAllocation &) = delete;
    DeviceAllocation &operator=(const DeviceAllocation &) = delete;

    Device       &device() const;
    Allocation   &allocation() const;
    SharedObject *shared() const;
    /* The points the device named for presents from this allocation
     * (NextPointRecord), oldest first, 0 standing for "read my pages". A
     * present takes the newest - the one named for it - and every older one
     * is of a present that never reached the kernel; all are forgotten when
     * the present has done. A point named over a full queue drops the oldest.
     * Guarded by the DeviceTable's lock, which every caller holds. */
    static constexpr uint32_t most_points = 8u;
    void          name_point(uint32_t point);
    /* A point named for a present that did not happen; false when not queued. */
    bool          withdraw_point(uint32_t point);
    /* The newest point named and not yet forgotten, 0 when none. */
    uint32_t      point() const;
    uint32_t      queued() const;
    void          forget_point();
    /* A point was ever named: presents of this allocation are its points'. */
    bool          pointed() const;
    /* What a present reads, the starvation noted once. */
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

}  // namespace av

#endif  // AV_DEVICE_ALLOCATION_HPP
