#include "harness/executor/vulkan_backend_probe.hpp"

#include "executor/import_table.hpp"
#include "proto/vulkan/descriptor_template_layout.hpp"
#include "proto/generated/vulkan/vk_dispatch_server.h"

namespace av {

VulkanBackendProbe::VulkanBackendProbe(VulkanBackend &target) : backend(target) {}

bool VulkanBackendProbe::run_through(vkproto::ServerDispatch &driver, uint32_t command_id,
                                     WireReader &payload, WireWriter &reply, WireArena &arena)
{
    const bool decoded = vkproto::dispatch_command(driver, command_id, payload, reply, arena);
    backend.parent_ = 0u;
    if (decoded) backend.ran_++;
    return decoded;
}

const HostObjectRegistry<uint64_t, VulkanObjectKind, uint64_t> &VulkanBackendProbe::handles() const { return backend.handles_; }

uint32_t VulkanBackendProbe::imports_held() const { return backend.imports_ != nullptr ? backend.imports_->count() : 0u; }

void VulkanBackendProbe::learn_types(VkPhysicalDevice physical) { backend.learn_types(physical); }

const GuestMemoryTypes &VulkanBackendProbe::types() const { return backend.types_; }

void VulkanBackendProbe::show_types(const VkPhysicalDeviceMemoryProperties &host, uint32_t backed)
{
    backend.types_.learn(host, backed);
    backend.types_learnt_ = true;
}

bool VulkanBackendProbe::memory_import() const { return backend.memory_import_; }

void VulkanBackendProbe::begin_frame(bool recording) { backend.guard_.begin_frame(recording); }

Status VulkanBackendProbe::add_template(uint64_t made, uint64_t device, const VkDescriptorUpdateTemplateEntry *entries,
                                        uint32_t count, const uint32_t *immutable, uint32_t immutable_count)
{
    DescriptorTemplateLayout layout;
    const Status measured = layout.take(entries, count);
    if (measured != Status::ok) return measured;
    const Status taken = backend.templates_.take(made, device, entries, count, immutable, immutable_count);
    if (taken != Status::ok) return taken;
    backend.made_through(device);
    backend.keep(made, VulkanObjectKind::descriptor_template, layout.bytes());
    return Status::ok;
}

bool VulkanBackendProbe::template_named(VkDescriptorUpdateTemplate made, const void *data, uint64_t set,
                                        uint64_t device)
{
    return backend.template_named(made, data, set, device);
}

VulkanGuard &VulkanBackendProbe::guard() { return backend.guard_; }

}
