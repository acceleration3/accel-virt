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
    [[nodiscard]] Status destroy(void *device);
    [[nodiscard]] Status open(void *device, DxgkargOpenAllocationRecord &request);
    [[nodiscard]] Status close(void *device, const DxgkargCloseAllocationRecord &request);
    Device              *find(void *device);
    DeviceAllocation    *opened(void *handle);
    [[nodiscard]] Status name_point(void *process, uint64_t surface, uint32_t point, NextPointRecord::Action action);
    [[nodiscard]] Status peek_point(void *handle, PointView &view);
    void                 consume_point(void *handle);
    uint32_t             devices() const;
    uint32_t             opens() const;

    void report(StateWriter &writer);

    bool invariants_hold() const;

private:
    [[nodiscard]] Status resolve(DxgkargOpenAllocationRecord &request);
    [[nodiscard]] Status open_one(Device &device, struct DxgkOpenAllocationInfoRecord &info);
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

}

#endif
