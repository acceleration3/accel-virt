#include "device/module/device_module.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

DeviceInstance *DeviceModule::create(const RvkHostOps *ops, const RvkExecutorOps *executor, const char *host_gpu,
                                     bool base_blobs, char *error, size_t error_size)
{
    AV_SCOPE("DeviceModule::create");
    TextBuffer message(error, error_size);
    if (ops == nullptr) {
        message.append("virtio-gpu-rvk: no host operations");
        return nullptr;
    }
    for (ObjectStorage<DeviceInstance> &slot : slots_) {
        if (slot.construct(*ops) != Status::ok) continue;   /* occupied */
        if (slot.object().device().realize(host_gpu, base_blobs, executor, message) == Status::ok) return &slot.object();
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

uint32_t DeviceModule::live() const
{
    AV_SCOPE("DeviceModule::live");
    uint32_t count = 0u;
    for (const ObjectStorage<DeviceInstance> &slot : slots_) count += slot.constructed() ? 1u : 0u;
    return count;
}

}  // namespace av
