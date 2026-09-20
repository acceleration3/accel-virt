/* The devices of this process, and the only way back from the block the
 * runtime holds (contracts/umd_interface.md, "The device object model").
 *
 * `device_of` is the guard on every device entry point: it reads the magic of
 * a block this driver does not own, then asks the pool whether the address in
 * it is still a live device, and only then may a field be read. A block
 * another component scribbled on, a block of a destroyed device and a pointer
 * never handed out all answer null. */
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
}  // namespace av

namespace av::icd {
class LogSink;
}  // namespace av::icd

namespace av::d3d {

class UmdAdapter;

struct DeviceTableProbe;

class DeviceTable {
public:
    DeviceTable(BlockSource &memory, icd::LogSink &log);
    DeviceTable(const DeviceTable &) = delete;
    DeviceTable &operator=(const DeviceTable &) = delete;

    /* What pfnCalcPrivateDeviceSize answers, whatever the runtime asked with. */
    static constexpr size_t private_bytes = sizeof(DeviceCookie);

    /* Makes a device on `adapter` and writes its cookie into `block`, the
     * memory the runtime allocated. invalid_parameter for a null block;
     * no_memory when the pool has none. The adapter's device count rises with
     * the device. */
    [[nodiscard]] Status make(UmdDevice *&made, UmdAdapter &adapter, void *block);
    /* The device `block` names, or null. */
    UmdDevice           *device_of(void *block) const;
    /* Destroys the device `block` names and clears its cookie.
     * invalid_parameter for a block naming none, which is what a double
     * destroy and a forged handle both look like. */
    [[nodiscard]] Status destroy(void *block);
    uint32_t             live_devices() const;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    icd::LogSink         &log_;
    ObjectPool<UmdDevice> devices_;

    friend struct DeviceTableProbe;
};

}  // namespace av::d3d

#endif  // AV_D3D_DEVICE_TABLE_HPP
