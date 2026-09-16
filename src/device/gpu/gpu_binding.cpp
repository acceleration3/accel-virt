#include "device/gpu/gpu_binding.hpp"

#include "core/trace/function_scope.hpp"
#include "device/gpu/device_uuid.hpp"
#include "device/host/host.hpp"
#include "core/support/text_buffer.hpp"
#include "proto/context/gpu_identity.hpp"

namespace av {

GpuBinding::GpuBinding(Host &host) : host_(host), index_(0u), bound_(false)
{
    AV_SCOPE("GpuBinding::GpuBinding");
}

bool GpuBinding::is_auto(const char *selection) const
{
    AV_SCOPE("GpuBinding::is_auto");
    const char expected[] = "auto";
    for (uint32_t position = 0u; position < sizeof expected; position++)
        if (selection[position] != expected[position]) return false;
    return true;
}

Status GpuBinding::bind(const char *selection, TextBuffer &message)
{
    AV_SCOPE("GpuBinding::bind");
    host_.require_main_loop("GpuBinding::bind");
    bound_ = false;
    if (selection != nullptr && !is_auto(selection)) return bind_uuid(selection, message);
    if (host_.gpu_count() == 0u) {
        message.append("virtio-gpu-rvk: the host has no Vulkan device");
        return refuse(Status::not_found, message);
    }
    return take(0u, message);
}

Status GpuBinding::bind_uuid(const char *selection, TextBuffer &message)
{
    AV_SCOPE("GpuBinding::bind_uuid");
    DeviceUuid wanted;
    if (wanted.parse(selection) != Status::ok) {
        message.append("virtio-gpu-rvk: host-gpu '");
        message.append(selection);
        message.append("' is neither auto nor a device UUID");
        return refuse(Status::invalid_parameter, message);
    }
    const uint32_t count = host_.gpu_count();
    for (uint32_t index = 0u; index < count; index++) {
        if (!wanted.equals(host_.gpu_identity(index).uuid())) continue;
        return take(index, message);
    }
    message.append("virtio-gpu-rvk: no host Vulkan device has UUID ");
    message.append(selection);
    return refuse(Status::not_found, message);
}

/* The glue creates what it needs on the physical device this names, so a host
 * that cannot take the GPU leaves the device unbound rather than running on
 * whichever one the glue would have picked. */
Status GpuBinding::take(uint32_t index, TextBuffer &message)
{
    AV_SCOPE("GpuBinding::take");
    const Status taken = host_.bind_gpu(index);
    if (taken != Status::ok) {
        message.append("virtio-gpu-rvk: the host refused host GPU ");
        message.append_decimal(index);
        return refuse(taken, message);
    }
    index_ = index;
    bound_ = true;
    return Status::ok;
}

Status GpuBinding::refuse(Status status, TextBuffer &message)
{
    AV_SCOPE("GpuBinding::refuse");
    host_.log(message.text());
    return status;
}

bool GpuBinding::bound() const
{
    AV_SCOPE("GpuBinding::bound");
    return bound_;
}

uint32_t GpuBinding::index() const
{
    AV_SCOPE("GpuBinding::index");
    return index_;
}

}  // namespace av
