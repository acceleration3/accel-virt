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

    VkPhysicalDevice find(VkInstance instance, const HostGpu &gpu);

private:
    static constexpr uint32_t max_devices = 16u;

    VkPhysicalDevice listed_[max_devices];
};

}

#endif
