#include "device/gpu/gpu_binding.hpp"

#include "device/gpu/device_uuid.hpp"
#include "device/host/host.hpp"
#include "device/support/text_buffer.hpp"

namespace av {

GpuBinding::GpuBinding(Host &host) : host_(host), index_(0u), bound_(false) {}

bool GpuBinding::is_auto(const char *selection) const
{
    const char expected[] = "auto";
    for (uint32_t position = 0u; position < sizeof expected; position++)
        if (selection[position] != expected[position]) return false;
    return true;
}

Status GpuBinding::bind(const char *selection, TextBuffer &message)
{
    host_.require_main_loop("GpuBinding::bind");
    bound_ = false;
    if (selection != nullptr && !is_auto(selection)) return bind_uuid(selection, message);
    if (host_.gpu_count() == 0u) {
        message.append("virtio-gpu-rvk: the host has no Vulkan device");
        return refuse(Status::not_found, message);
    }
    index_ = 0u;
    bound_ = true;
    return Status::ok;
}

Status GpuBinding::bind_uuid(const char *selection, TextBuffer &message)
{
    DeviceUuid wanted;
    if (wanted.parse(selection) != Status::ok) {
        message.append("virtio-gpu-rvk: host-gpu '");
        message.append(selection);
        message.append("' is neither auto nor a device UUID");
        return refuse(Status::invalid_parameter, message);
    }
    const uint32_t count = host_.gpu_count();
    for (uint32_t index = 0u; index < count; index++) {
        uint8_t uuid[DeviceUuid::bytes] = {};
        host_.gpu_uuid(index, uuid);
        if (!wanted.equals(uuid)) continue;
        index_ = index;
        bound_ = true;
        return Status::ok;
    }
    message.append("virtio-gpu-rvk: no host Vulkan device has UUID ");
    message.append(selection);
    return refuse(Status::not_found, message);
}

Status GpuBinding::refuse(Status status, TextBuffer &message)
{
    host_.log(message.text());
    return status;
}

bool GpuBinding::bound() const
{
    return bound_;
}

uint32_t GpuBinding::index() const
{
    return index_;
}

}  // namespace av
