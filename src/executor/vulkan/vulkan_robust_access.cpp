#include "executor/vulkan/vulkan_robust_access.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

VulkanRobustAccess::VulkanRobustAccess() : features_{}
{
    AV_SCOPE("VulkanRobustAccess::VulkanRobustAccess");
}

bool VulkanRobustAccess::robust_features(VkPhysicalDeviceFeatures &features, const VkPhysicalDeviceFeatures &offered) const
{
    AV_SCOPE("VulkanRobustAccess::robust_features");
    if (offered.robustBufferAccess == VK_FALSE) return false;
    features.robustBufferAccess = VK_TRUE;
    return true;
}

bool VulkanRobustAccess::apply(VkPhysicalDevice physical, VkDeviceCreateInfo &create)
{
    AV_SCOPE("VulkanRobustAccess::apply");
    VkPhysicalDeviceFeatures offered = {};
    ::vkGetPhysicalDeviceFeatures(physical, &offered);
    bool applied = false;
    bool carried = false;
    for (auto *link = static_cast<const VkBaseInStructure *>(create.pNext); link != nullptr; link = link->pNext) {
        switch (link->sType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2: {
            auto *features = const_cast<VkPhysicalDeviceFeatures2 *>(reinterpret_cast<const VkPhysicalDeviceFeatures2 *>(link));
            applied = robust_features(features->features, offered) || applied;
            carried = true;
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES: {
            const_cast<VkPhysicalDeviceImageRobustnessFeatures *>(
                reinterpret_cast<const VkPhysicalDeviceImageRobustnessFeatures *>(link))->robustImageAccess = VK_TRUE;
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES: {
            const_cast<VkPhysicalDeviceVulkan13Features *>(
                reinterpret_cast<const VkPhysicalDeviceVulkan13Features *>(link))->robustImageAccess = VK_TRUE;
            break;
        }
        default: break;
        }
    }
    if (carried) return applied;
    features_ = create.pEnabledFeatures != nullptr ? *create.pEnabledFeatures : VkPhysicalDeviceFeatures{};
    if (!robust_features(features_, offered)) return false;
    create.pEnabledFeatures = &features_;
    return true;
}

}
