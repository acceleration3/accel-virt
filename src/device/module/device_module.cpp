#include "device/module/device_module.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av {

DeviceInstance *DeviceModule::create(const AvHostOps *ops, const AvExecutorOps *executor, const char *host_gpu,
                                     bool base_blobs, const AvRecordingOptions *recording,
                                     const char *renderer, const char *executor_path, char *error,
                                     size_t error_size)
{
    AV_SCOPE("DeviceModule::create");
    TextBuffer message(error, error_size);
    if (ops == nullptr) {
        message.append("virtio-gpu-av: no host operations");
        return nullptr;
    }
    if (!lendings_.constructed()) {
        static_cast<void>(blocks_.construct());
        static_cast<void>(lendings_.construct(blocks_.object()));
    }
    for (ObjectStorage<DeviceInstance> &slot : slots_) {
        if (slot.construct(*ops) != Status::ok) continue;   /* occupied */
        slot.object().device().share_lendings(lendings_.object());
        if (slot.object().device().realize(host_gpu, base_blobs, executor, recording, renderer, executor_path,
                                           message) == Status::ok)
            return &slot.object();
        slot.destroy();
        release_lendings();
        return nullptr;
    }
    message.append("virtio-gpu-av: every device slot is in use");
    return nullptr;
}

Status DeviceModule::destroy(DeviceInstance *instance)
{
    AV_SCOPE("DeviceModule::destroy");
    for (ObjectStorage<DeviceInstance> &slot : slots_) {
        if (!slot.constructed() || &slot.object() != instance) continue;
        slot.object().device().reset();
        slot.destroy();
        release_lendings();
        return Status::ok;
    }
    return Status::invalid_parameter;
}

void DeviceModule::release_lendings()
{
    AV_SCOPE("DeviceModule::release_lendings");
    if (live() != 0u) return;
    lendings_.destroy();
    blocks_.destroy();
}

void DeviceModule::report(StateWriter &writer)
{
    AV_SCOPE("DeviceModule::report");
    writer.open("module", 0u);
    writer.number("devices", live());
    if (lendings_.constructed()) {
        writer.number("lendings", lendings_.object().live());
        writer.number("lent_pages", lendings_.object().pages());
    }
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
