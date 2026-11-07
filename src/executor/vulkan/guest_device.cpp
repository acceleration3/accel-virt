#include "executor/vulkan/guest_device.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

GuestDevice::GuestDevice() : table_(), memory_(table_), invalid_shader_extension_(false)
{
    AV_SCOPE("GuestDevice::GuestDevice");
}

bool GuestDevice::load(VkInstance instance, VkPhysicalDevice physical, VkDevice device)
{
    AV_SCOPE("GuestDevice::load");
    return table_.load(instance, physical, device);
}

VkDevice GuestDevice::device() const
{
    AV_SCOPE("GuestDevice::device");
    return table_.device();
}

VulkanDeviceTable &GuestDevice::table()
{
    AV_SCOPE("GuestDevice::table");
    return table_;
}

VulkanMemoryImport &GuestDevice::memory()
{
    AV_SCOPE("GuestDevice::memory");
    return memory_;
}

void GuestDevice::note_invalid_shader_extension(bool enabled)
{
    AV_SCOPE("GuestDevice::note_invalid_shader_extension");
    invalid_shader_extension_ = enabled;
}

VkResult GuestDevice::shader_refusal() const
{
    AV_SCOPE("GuestDevice::shader_refusal");
    return invalid_shader_extension_ ? VK_ERROR_INVALID_SHADER_NV : VK_ERROR_UNKNOWN;
}

}
