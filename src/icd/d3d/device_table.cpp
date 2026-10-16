#include "icd/d3d/device_table.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/d3d/umd_adapter.hpp"

namespace av::d3d {

DeviceTable::DeviceTable(BlockSource &memory) : devices_(memory)
{
    AV_SCOPE("DeviceTable::DeviceTable");
}

Status DeviceTable::make(UmdDevice *&made, UmdAdapter &adapter, void *block)
{
    AV_SCOPE("DeviceTable::make");
    made = nullptr;
    if (block == nullptr) return Status::invalid_parameter;
    UmdDevice   *device = nullptr;
    const Status created = devices_.create(device, adapter);
    if (created != Status::ok) return created;
    DeviceCookie *const cookie = static_cast<DeviceCookie *>(block);
    cookie->clear();
    cookie->write(*device);
    adapter.device_made();
    made = device;
    return Status::ok;
}

UmdDevice *DeviceTable::device_of(void *block) const
{
    AV_SCOPE("DeviceTable::device_of");
    if (block == nullptr) return nullptr;
    UmdDevice *const named = static_cast<const DeviceCookie *>(block)->named();
    if (named == nullptr) return nullptr;
    return devices_.find(named);
}

Status DeviceTable::destroy(void *block)
{
    AV_SCOPE("DeviceTable::destroy");
    UmdDevice *const device = device_of(block);
    if (device == nullptr) return Status::invalid_parameter;
    device->adapter().device_gone();
    static_cast<DeviceCookie *>(block)->clear();
    return devices_.destroy(device);
}

uint32_t DeviceTable::live_devices() const
{
    AV_SCOPE("DeviceTable::live_devices");
    return devices_.live();
}

void DeviceTable::report(StateWriter &writer) const
{
    AV_SCOPE("DeviceTable::report");
    uint32_t index = 0u;
    devices_.for_each([&writer, &index](const UmdDevice &device) {
        device.report(writer, index);
        index++;
    });
}

bool DeviceTable::invariants_hold() const
{
    AV_SCOPE("DeviceTable::invariants_hold");
    return devices_.all_of([](const UmdDevice &device) { return device.invariants_hold(); });
}

}  // namespace av::d3d
