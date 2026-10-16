#include "executor/shim/physical_device_identity.hpp"

#include "core/trace/function_scope.hpp"

#include <vulkan/vulkan.h>

namespace av {

PhysicalDeviceIdentity::PhysicalDeviceIdentity(VkInstance instance, VkPhysicalDevice physical)
    : uuid_(), render_minor_(-1), known_(false)
{
    AV_SCOPE("PhysicalDeviceIdentity::PhysicalDeviceIdentity");
    auto query = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2"));
    if (query == nullptr || physical == VK_NULL_HANDLE) return;
    VkPhysicalDeviceDrmPropertiesEXT node = {};
    node.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT;
    VkPhysicalDeviceIDProperties identity = {};
    identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
    identity.pNext = &node;
    VkPhysicalDeviceProperties2 properties = {};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &identity;
    query(physical, &properties);
    uuid_ = DeviceUuid(identity.deviceUUID);
    render_minor_ = node.hasRender ? static_cast<int32_t>(node.renderMinor) : -1;
    known_ = true;
}

const DeviceUuid &PhysicalDeviceIdentity::uuid() const
{
    AV_SCOPE("PhysicalDeviceIdentity::uuid");
    return uuid_;
}

int32_t PhysicalDeviceIdentity::render_minor() const
{
    AV_SCOPE("PhysicalDeviceIdentity::render_minor");
    return render_minor_;
}

bool PhysicalDeviceIdentity::is(const uint8_t *uuid) const
{
    AV_SCOPE("PhysicalDeviceIdentity::is");
    return known_ && uuid_.equals(uuid);
}

}  // namespace av
