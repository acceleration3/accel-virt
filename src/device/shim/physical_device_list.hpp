/* The physical devices one VkInstance enumerates, and which of them is a given
 * host GPU.
 *
 * The driver's half of VulkanBackend's enumeration answers (feature 044, T034):
 * a guest context is shown the one physical device its GPU is and no other.
 * What is here is only the two calls the loader answers; what the guest is told
 * from the result stays in the backend, where a test can judge it. */
#ifndef AV_PHYSICAL_DEVICE_LIST_HPP
#define AV_PHYSICAL_DEVICE_LIST_HPP

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class HostGpu;

class PhysicalDeviceList {
public:
    PhysicalDeviceList();
    PhysicalDeviceList(const PhysicalDeviceList &) = delete;
    PhysicalDeviceList &operator=(const PhysicalDeviceList &) = delete;

    /* The physical device of `instance` whose deviceUUID is `gpu`'s, or
     * VK_NULL_HANDLE for an instance that lists none. Asked each time: a guest
     * may make several instances, and a physical device is one instance's. */
    VkPhysicalDevice find(VkInstance instance, const HostGpu &gpu);

private:
    /* This host enumerates two; nobody will meet sixteen. */
    static constexpr uint32_t max_devices = 16u;

    VkPhysicalDevice listed_[max_devices];
};

}  // namespace av

#endif  // AV_PHYSICAL_DEVICE_LIST_HPP
