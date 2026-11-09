#ifndef AV_VULKAN_ROBUST_ACCESS_HPP
#define AV_VULKAN_ROBUST_ACCESS_HPP

#include <vulkan/vulkan.h>

#include <stdint.h>

namespace av {

class VulkanRobustAccess {
public:
    VulkanRobustAccess();
    VulkanRobustAccess(const VulkanRobustAccess &) = delete;
    VulkanRobustAccess &operator=(const VulkanRobustAccess &) = delete;

    bool apply(VkPhysicalDevice physical, VkDeviceCreateInfo &create);
    void hide_capture_replay(const void *chain);
    uint32_t hidden() const;

private:
    void hide(VkBool32 &feature);
    bool robust_features(VkPhysicalDeviceFeatures &features, const VkPhysicalDeviceFeatures &offered) const;

    VkPhysicalDeviceFeatures features_;
    uint32_t                 hidden_;
};

}

#endif
