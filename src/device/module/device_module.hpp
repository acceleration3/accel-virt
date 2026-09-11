/* Every device the loaded QEMU module created. QEMU's C glue cannot hold a C++
 * object, so the module keeps its devices in the ABI translation unit's static
 * storage - the device-side counterpart of the kernel shim's root - and hands
 * the glue an opaque handle. One device per host GPU bounds the count.
 *
 * Realize, reset and finalize all run on the main loop under QEMU's big lock,
 * so the slots need no lock of their own. */
#ifndef AV_DEVICE_MODULE_HPP
#define AV_DEVICE_MODULE_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "device/abi/rvk_device.h"
#include "device/module/device_instance.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct DeviceModuleProbe;

class DeviceModule {
public:
    static constexpr uint32_t max_devices = 16u;

    constexpr DeviceModule() = default;
    DeviceModule(const DeviceModule &) = delete;
    DeviceModule &operator=(const DeviceModule &) = delete;

    /* A realized device, or null with the reason in `error`. */
    DeviceInstance      *create(const RvkHostOps *ops, const char *host_gpu, char *error, size_t error_size);
    [[nodiscard]] Status destroy(DeviceInstance *instance);
    uint32_t             live() const;

private:
    ObjectStorage<DeviceInstance> slots_[max_devices] = {};

    friend struct DeviceModuleProbe;
};

}  // namespace av

#endif  // AV_DEVICE_MODULE_HPP
