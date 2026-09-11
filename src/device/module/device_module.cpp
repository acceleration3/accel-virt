#include "device/module/device_module.hpp"

#include "device/support/text_buffer.hpp"

namespace av {

DeviceInstance *DeviceModule::create(const RvkHostOps *ops, const char *host_gpu, bool base_blobs, char *error,
                                     size_t error_size)
{
    TextBuffer message(error, error_size);
    if (ops == nullptr) {
        message.append("virtio-gpu-rvk: no host operations");
        return nullptr;
    }
    for (ObjectStorage<DeviceInstance> &slot : slots_) {
        if (slot.construct(*ops) != Status::ok) continue;   /* occupied */
        if (slot.object().device().realize(host_gpu, base_blobs, message) == Status::ok) return &slot.object();
        slot.destroy();
        return nullptr;
    }
    message.append("virtio-gpu-rvk: every device slot is in use");
    return nullptr;
}

Status DeviceModule::destroy(DeviceInstance *instance)
{
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
    uint32_t count = 0u;
    for (const ObjectStorage<DeviceInstance> &slot : slots_) count += slot.constructed() ? 1u : 0u;
    return count;
}

}  // namespace av
