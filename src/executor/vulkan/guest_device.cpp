#include "executor/vulkan/guest_device.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

GuestDevice::GuestDevice() : table_(), memory_(table_)
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

}  // namespace av
