#ifndef AV_VULKAN_TIMELINE_FEATURE_HPP
#define AV_VULKAN_TIMELINE_FEATURE_HPP

#include <vulkan/vulkan.h>

namespace av {

class VulkanTimelineFeature {
public:
    VulkanTimelineFeature();
    VulkanTimelineFeature(const VulkanTimelineFeature &) = delete;
    VulkanTimelineFeature &operator=(const VulkanTimelineFeature &) = delete;

    bool offered_by(VkPhysicalDevice physical) const;
    bool apply(VkDeviceCreateInfo &create);

private:
    VkPhysicalDeviceTimelineSemaphoreFeatures feature_;
};

}

#endif
