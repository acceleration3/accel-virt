#include "device/shim/physical_device_identity.hpp"

#include "core/trace/function_scope.hpp"

#include <string.h>
#include <vulkan/vulkan.h>

namespace av {

PhysicalDeviceIdentity::PhysicalDeviceIdentity(VkInstance instance, VkPhysicalDevice physical)
    : uuid_(), known_(false)
{
    AV_SCOPE("PhysicalDeviceIdentity::PhysicalDeviceIdentity");
    auto query = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2"));
    if (query == nullptr || physical == VK_NULL_HANDLE) return;
    VkPhysicalDeviceIDProperties identity = {};
    identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
    VkPhysicalDeviceProperties2 properties = {};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &identity;
    query(physical, &properties);
    memcpy(uuid_, identity.deviceUUID, uuid_bytes);
    known_ = true;
}

bool PhysicalDeviceIdentity::is(const uint8_t *uuid) const
{
    AV_SCOPE("PhysicalDeviceIdentity::is");
    return known_ && uuid != nullptr && memcmp(uuid_, uuid, uuid_bytes) == 0;
}

}  // namespace av
