#ifndef AV_D3D_DEVICE_TABLE_HPP
#define AV_D3D_DEVICE_TABLE_HPP

#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "icd/d3d/device_cookie.hpp"
#include "icd/d3d/umd_device.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
class StateWriter;
}


namespace av::d3d {

class UmdAdapter;

struct DeviceTableProbe;

class DeviceTable {
public:
    explicit DeviceTable(BlockSource &memory);
    DeviceTable(const DeviceTable &) = delete;
    DeviceTable &operator=(const DeviceTable &) = delete;

    static constexpr size_t extension_offset = sizeof(DeviceCookie);
    static constexpr size_t private_bytes = extension_offset + sizeof(void *);

    [[nodiscard]] Status make(UmdDevice *&made, UmdAdapter &adapter, void *block);
    UmdDevice           *device_of(void *block) const;
    [[nodiscard]] Status destroy(void *block);
    uint32_t             live_devices() const;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    ObjectPool<UmdDevice> devices_;

    friend struct DeviceTableProbe;
};

}

#endif
