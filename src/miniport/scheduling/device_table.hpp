/* The adapter's devices and the allocations each has open. Every handle dxgkrnl
 * passes back is checked against the pool it must come from, and every list is
 * all-or-nothing. Takes its mutex before the allocation table's. */
#ifndef AV_DEVICE_TABLE_HPP
#define AV_DEVICE_TABLE_HPP

#include "miniport/memory/device_allocation.hpp"
#include "miniport/memory/point_view.hpp"
#include "proto/escape/next_point_record.hpp"
#include "core/platform/mutex.hpp"
#include "core/platform/platform.hpp"
#include "miniport/scheduling/device.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class StateWriter;

class Adapter;
class Allocation;
class AllocationTable;
class SharedAllocationData;
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
    /* `point` named on an open of the allocation minted as `surface` by a
     * device of `process`, the hKmdProcess CreateProcess answered
     * (NextPointRecord): invalid_parameter for no process, not_found where no
     * device of it has such an open. */
    [[nodiscard]] Status name_point(void *process, uint64_t surface, uint32_t point, NextPointRecord::Action action);
    /* What a present of the allocation `handle` names reads of its points,
     * under the table's lock the escapes naming them take; not_found for a
     * handle this table did not open. */
    [[nodiscard]] Status peek_point(void *handle, PointView &view);
    /* The present of `handle` is done: every point waiting for it goes. */
    void                 consume_point(void *handle);
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
    /* The creating device's own open, where the minted record is written into
     * the bytes dxgkrnl copies out (research.md §15). */
    [[nodiscard]] Status adopt(const Allocation &allocation, const struct DxgkOpenAllocationInfoRecord &info,
                               SharedAllocationData &data, SharedObject *&object);
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
