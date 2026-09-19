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

class StateWriter;

class Adapter;
class Allocation;
class AllocationTable;
class SharedObject;
struct DeviceTableProbe;
class DxgkrnlInterface;
class SharedObjectTable;
struct DxgkargCloseAllocationRecord;
struct DxgkargCreateDeviceRecord;
struct DxgkargOpenAllocationRecord;

class DeviceTable {
public:
    DeviceTable(Platform &platform, AllocationTable &allocations, SharedObjectTable &shared,
                DxgkrnlInterface &dxgkrnl);
    DeviceTable(const DeviceTable &) = delete;
    DeviceTable &operator=(const DeviceTable &) = delete;

    [[nodiscard]] Status create(Adapter &adapter, DxgkargCreateDeviceRecord &request);
    /* Closes whatever the device still has open. */
    [[nodiscard]] Status destroy(void *device);
    [[nodiscard]] Status open(void *device, DxgkargOpenAllocationRecord &request);
    [[nodiscard]] Status close(void *device, const DxgkargCloseAllocationRecord &request);
    /* The live device `device` names, or null. */
    Device              *find(void *device);
    /* The allocation `handle` names, if this table opened it; null otherwise.
     * What a present's allocation list is read through (feature 046). */
    DeviceAllocation    *opened(void *handle);
    uint32_t             devices() const;
    uint32_t             opens() const;

    /* A line for the table and one per device with the allocations it has
     * open; `busy=true` and nothing more while another thread holds the table. */
    void report(StateWriter &writer);

    bool invariants_hold() const;

private:
    [[nodiscard]] Status resolve(DxgkargOpenAllocationRecord &request);
    [[nodiscard]] Status open_one(Device &device, struct DxgkOpenAllocationInfoRecord &info);
    /* The shared object `info`'s private data names, for an allocation the
     * adapter gave an identity to; null, with ok, for one it did not
     * (feature 053). The record is the creating process's own bytes, which is
     * what dxgkrnl hands the opening process. */
    [[nodiscard]] Status claim(const Allocation &allocation, const struct DxgkOpenAllocationInfoRecord &info,
                               SharedObject *&object);
    void                 close_opened(DxgkargOpenAllocationRecord &request, uint32_t count);

    Platform                    &platform_;
    AllocationTable             &allocations_;
    SharedObjectTable           &shared_;
    DxgkrnlInterface            &dxgkrnl_;
    Mutex                        mutex_;
    ObjectPool<Device>           devices_;
    ObjectPool<DeviceAllocation> opens_;

    friend struct DeviceTableProbe;
};

}  // namespace av

#endif  // AV_DEVICE_TABLE_HPP
