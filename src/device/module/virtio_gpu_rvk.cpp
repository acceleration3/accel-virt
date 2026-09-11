#include "device/module/virtio_gpu_rvk.hpp"

#include "device/host/host.hpp"

namespace av {

VirtioGpuRvk::VirtioGpuRvk(Host &host) : host_(host), binding_(host), contexts_(host) {}

Status VirtioGpuRvk::realize(const char *host_gpu, TextBuffer &message)
{
    host_.require_main_loop("VirtioGpuRvk::realize");
    if (binding_.bound()) return Status::invalid_state;
    return binding_.bind(host_gpu, message);
}

void VirtioGpuRvk::reset()
{
    host_.require_main_loop("VirtioGpuRvk::reset");
    contexts_.destroy_all();
}

Status VirtioGpuRvk::create_context(uint32_t id)
{
    host_.require_main_loop("VirtioGpuRvk::create_context");
    if (!binding_.bound()) return Status::invalid_state;
    return contexts_.create(id);
}

Status VirtioGpuRvk::destroy_context(uint32_t id)
{
    host_.require_main_loop("VirtioGpuRvk::destroy_context");
    return contexts_.destroy(id);
}

uint32_t VirtioGpuRvk::contexts() const
{
    return contexts_.live();
}

bool VirtioGpuRvk::realized() const
{
    return binding_.bound();
}

uint32_t VirtioGpuRvk::gpu_index() const
{
    return binding_.index();
}

/* Contexts exist only on a realized device. */
bool VirtioGpuRvk::invariants_hold() const
{
    return contexts_.invariants_hold() && (binding_.bound() || contexts_.live() == 0u);
}

}  // namespace av
