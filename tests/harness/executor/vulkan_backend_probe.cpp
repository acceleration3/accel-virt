#include "harness/executor/vulkan_backend_probe.hpp"

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

void VulkanBackendProbe::name_template(VulkanBackend &target, VkDescriptorUpdateTemplate made)
{
    target.templates_.name(made);
}

}  // namespace av
