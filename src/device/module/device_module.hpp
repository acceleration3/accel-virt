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

    DeviceInstance      *create(const AvHostOps *ops, const AvExecutorOps *executor, const char *host_gpu,
                                bool base_blobs, const AvRecordingOptions *recording, const char *renderer,
                                const char *executor_path, uint32_t vram_reserve_mib, char *error, size_t error_size);
    [[nodiscard]] Status destroy(DeviceInstance *instance);
    uint32_t             live() const;
    void                 report(StateWriter &writer) override;

private:
    void release_lendings();

    ObjectStorage<ProcessBlockSource> blocks_;
    ObjectStorage<GuestRamLendings>   lendings_;
    ObjectStorage<DeviceInstance>     slots_[max_devices] = {};

    friend struct DeviceModuleProbe;
};

}

#endif
