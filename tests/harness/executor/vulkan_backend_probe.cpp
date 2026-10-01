#include "harness/executor/vulkan_backend_probe.hpp"

#include "device/abi/av_executor.h"

namespace av {

VulkanBackendProbe::VulkanBackendProbe(VulkanBackend &target) : backend(target) {}

bool VulkanBackendProbe::run_through(vkproto::ServerDispatch &driver, uint32_t command_id,
                                     WireReader &payload, WireWriter &reply, WireArena &arena)
{ return backend.run_through(driver, command_id, payload, reply, arena); }

const HandleRegistry &VulkanBackendProbe::handles() const { return backend.handles_; }

uint32_t VulkanBackendProbe::imports_held() const { return backend.import_.held(); }

Status VulkanBackendProbe::reserve_template(VkDevice device, const VkDescriptorUpdateTemplateEntry *entry)
{
    VkDescriptorUpdateTemplateCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
    info.descriptorUpdateEntryCount = 1u;
    info.pDescriptorUpdateEntries = entry;
    return backend.templates_.reserve(device, &info);
}

bool VulkanBackendProbe::holds_fd(int fd) const { return backend.fds_.holds(fd); }

VkResult VulkanBackendProbe::export_answered(VkResult answered, int &fd)
{
    return backend.remembered(backend.device_, answered, &fd);
}

void VulkanBackendProbe::name_template(VulkanBackend &target, VkDescriptorUpdateTemplate made)
{
    target.templates_.name(made);
}

uint32_t VulkanBackendProbe::decide_import(VkPhysicalDevice physical)
{
    backend.importable_.learn(0u, 0u, 0u);
    backend.decide_import(physical);
    return backend.importable_.kind();
}

void VulkanBackendProbe::import_as_pointer(VkPhysicalDevice physical)
{
    backend.importable_.learn(reinterpret_cast<uint64_t>(physical), 0u, backend.import_.probe_types(physical));
}

void VulkanBackendProbe::import_as_dmabuf(VkPhysicalDevice physical)
{
    /* A mask of one type, which is enough: what the backend does with the kind
     * is what this makes reachable, not which types that kind lands in. */
    backend.importable_.learn(reinterpret_cast<uint64_t>(physical), 1u, 0u);
}

}  // namespace av
