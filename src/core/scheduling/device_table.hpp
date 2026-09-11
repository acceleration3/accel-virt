/* The adapter's devices and the allocations each has open. Every handle dxgkrnl
 * passes back is checked against the pool it must come from, and every list is
 * all-or-nothing. Takes its mutex before the allocation table's. */
#ifndef AV_DEVICE_TABLE_HPP
#define AV_DEVICE_TABLE_HPP

#include "core/memory/device_allocation.hpp"
#include "core/platform/mutex.hpp"
#include "core/platform/platform.hpp"
#include "core/scheduling/device.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Adapter;
class AllocationTable;
struct DeviceTableProbe;
class DxgkrnlInterface;
struct DxgkargCloseAllocationRecord;
struct DxgkargCreateDeviceRecord;
struct DxgkargOpenAllocationRecord;

class DeviceTable {
public:
    DeviceTable(Platform &platform, AllocationTable &allocations, DxgkrnlInterface &dxgkrnl);
    DeviceTable(const DeviceTable &) = delete;
    DeviceTable &operator=(const DeviceTable &) = delete;

    [[nodiscard]] Status create(Adapter &adapter, DxgkargCreateDeviceRecord &request);
    /* Closes whatever the device still has open. */
    [[nodiscard]] Status destroy(void *device);
    [[nodiscard]] Status open(void *device, DxgkargOpenAllocationRecord &request);
    [[nodiscard]] Status close(void *device, const DxgkargCloseAllocationRecord &request);
    /* The live device `device` names, or null. */
    Device              *find(void *device);
    uint32_t             devices() const;
    uint32_t             opens() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status resolve(DxgkargOpenAllocationRecord &request);
    [[nodiscard]] Status open_one(Device &device, struct DxgkOpenAllocationInfoRecord &info);
    void                 close_opened(DxgkargOpenAllocationRecord &request, uint32_t count);

    Platform                    &platform_;
    AllocationTable             &allocations_;
    DxgkrnlInterface            &dxgkrnl_;
    Mutex                        mutex_;
    ObjectPool<Device>           devices_;
    ObjectPool<DeviceAllocation> opens_;

    friend struct DeviceTableProbe;
};

}  // namespace av

#endif  // AV_DEVICE_TABLE_HPP
