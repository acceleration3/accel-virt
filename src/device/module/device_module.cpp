#include "device/module/device_module.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av {

DeviceInstance *DeviceModule::create(const RvkHostOps *ops, const RvkExecutorOps *executor, const char *host_gpu,
                                     bool base_blobs, const RvkRecordingOptions *recording,
                                     const char *renderer, const char *executor_path, char *error,
                                     size_t error_size)
{
    AV_SCOPE("DeviceModule::create");
    TextBuffer message(error, error_size);
    if (ops == nullptr) {
        message.append("virtio-gpu-rvk: no host operations");
        return nullptr;
    }
    for (ObjectStorage<DeviceInstance> &slot : slots_) {
        if (slot.construct(*ops) != Status::ok) continue;   /* occupied */
        if (slot.object().device().realize(host_gpu, base_blobs, executor, recording, renderer, executor_path,
                                           message) == Status::ok)
            return &slot.object();
        slot.destroy();
        return nullptr;
    }
    message.append("virtio-gpu-rvk: every device slot is in use");
    return nullptr;
}

Status DeviceModule::destroy(DeviceInstance *instance)
{
    AV_SCOPE("DeviceModule::destroy");
    for (ObjectStorage<DeviceInstance> &slot : slots_) {
        if (!slot.constructed() || &slot.object() != instance) continue;
        slot.object().device().reset();
        slot.destroy();
        return Status::ok;
    }
    return Status::invalid_parameter;
}

void DeviceModule::report(StateWriter &writer)
{
    AV_SCOPE("DeviceModule::report");
    writer.open("module", 0u);
    writer.number("devices", live());
    for (uint32_t slot = 0u; slot < max_devices; slot++) {
        if (!slots_[slot].constructed()) continue;
        writer.open("device", slot);
        slots_[slot].object().device().report(writer);
        writer.close();
    }
    writer.close();
}

uint32_t DeviceModule::live() const
{
    AV_SCOPE("DeviceModule::live");
    uint32_t count = 0u;
    for (const ObjectStorage<DeviceInstance> &slot : slots_) count += slot.constructed() ? 1u : 0u;
    return count;
}

}  // namespace av
