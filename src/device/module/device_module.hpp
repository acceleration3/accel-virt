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
#include "core/trace/state_reporter.hpp"
#include "device/abi/av_device.h"
#include "device/abi/av_executor.h"
#include "device/host/process_block_source.hpp"
#include "device/memory/guest_ram_lendings.hpp"
#include "device/module/device_instance.hpp"
#include "proto/context/av_device_count.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct DeviceModuleProbe;

class DeviceModule final : public StateReporter {
public:
    static constexpr uint32_t max_devices = most_av_devices;

    constexpr DeviceModule() = default;
    DeviceModule(const DeviceModule &) = delete;
    DeviceModule &operator=(const DeviceModule &) = delete;

    /* A realized device, or null with the reason in `error`. */
    DeviceInstance      *create(const AvHostOps *ops, const AvExecutorOps *executor, const char *host_gpu,
                                bool base_blobs, const AvRecordingOptions *recording, const char *renderer,
                                const char *executor_path, char *error, size_t error_size);
    [[nodiscard]] Status destroy(DeviceInstance *instance);
    uint32_t             live() const;
    /* Main loop: the module's line, then each live device by slot. */
    void                 report(StateWriter &writer) override;

private:
    /* The lendings gone once no device is left to hold one. */
    void release_lendings();

    /* Made with the first device and gone with the last: what the devices of
     * one process lend of the guest's RAM, which one device's lending may
     * outlive that device in (GuestRamLending). */
    ObjectStorage<ProcessBlockSource> blocks_;
    ObjectStorage<GuestRamLendings>   lendings_;
    ObjectStorage<DeviceInstance>     slots_[max_devices] = {};

    friend struct DeviceModuleProbe;
};

}  // namespace av

#endif  // AV_DEVICE_MODULE_HPP
