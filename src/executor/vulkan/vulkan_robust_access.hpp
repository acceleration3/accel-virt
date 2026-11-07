#ifndef AV_VULKAN_ROBUST_ACCESS_HPP
#define AV_VULKAN_ROBUST_ACCESS_HPP

#include <vulkan/vulkan.h>

namespace av {

class VulkanRobustAccess {
public:
    VulkanRobustAccess();
    VulkanRobustAccess(const VulkanRobustAccess &) = delete;
    VulkanRobustAccess &operator=(const VulkanRobustAccess &) = delete;

    bool apply(VkPhysicalDevice physical, VkDeviceCreateInfo &create);

private:
    bool robust_features(VkPhysicalDeviceFeatures &features, const VkPhysicalDeviceFeatures &offered) const;

    VkPhysicalDeviceFeatures features_;
};

}

#endif
