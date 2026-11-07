#include "executor/vulkan/vulkan_timeline_feature.hpp"

#include "core/trace/function_scope.hpp"

#include <string.h>

namespace av {

namespace {

constexpr uint32_t most_extensions = 512u;

}

VulkanTimelineFeature::VulkanTimelineFeature() : feature_{}
{
    AV_SCOPE("VulkanTimelineFeature::VulkanTimelineFeature");
}

bool VulkanTimelineFeature::offered_by(VkPhysicalDevice physical) const
{
    AV_SCOPE("VulkanTimelineFeature::offered_by");
    uint32_t count = 0u;
    if (::vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, nullptr) != VK_SUCCESS || count == 0u ||
        count > most_extensions)
        return false;
    VkExtensionProperties listed[most_extensions];
    if (::vkEnumerateDeviceExtensionProperties(physical, nullptr, &count, listed) != VK_SUCCESS) return false;
    for (uint32_t at = 0u; at < count; at++)
        if (strcmp(listed[at].extensionName, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME) == 0) return true;
    return false;
}

bool VulkanTimelineFeature::apply(VkDeviceCreateInfo &create)
{
    AV_SCOPE("VulkanTimelineFeature::apply");
    for (auto *link = static_cast<const VkBaseInStructure *>(create.pNext); link != nullptr; link = link->pNext) {
        if (link->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES) {
            const_cast<VkPhysicalDeviceVulkan12Features *>(reinterpret_cast<const VkPhysicalDeviceVulkan12Features *>(link))
                ->timelineSemaphore = VK_TRUE;
            return true;
        }
        if (link->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES) {
            const_cast<VkPhysicalDeviceTimelineSemaphoreFeatures *>(
                reinterpret_cast<const VkPhysicalDeviceTimelineSemaphoreFeatures *>(link))
                ->timelineSemaphore = VK_TRUE;
            return true;
        }
    }
    feature_ = {};
    feature_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    feature_.pNext = const_cast<void *>(create.pNext);
    feature_.timelineSemaphore = VK_TRUE;
    create.pNext = &feature_;
    return true;
}

}
