#include "executor/vulkan/vulkan_robust_access.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

VulkanRobustAccess::VulkanRobustAccess() : features_{}, hidden_(0u)
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

void VulkanRobustAccess::hide_capture_replay(const void *chain)
{
    AV_SCOPE("VulkanRobustAccess::hide_capture_replay");
    for (auto *link = static_cast<const VkBaseInStructure *>(chain); link != nullptr; link = link->pNext) {
        switch (link->sType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
            hide(const_cast<VkPhysicalDeviceVulkan12Features *>(reinterpret_cast<const VkPhysicalDeviceVulkan12Features *>(link))
                     ->bufferDeviceAddressCaptureReplay);
            break;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES:
            hide(const_cast<VkPhysicalDeviceBufferDeviceAddressFeatures *>(
                     reinterpret_cast<const VkPhysicalDeviceBufferDeviceAddressFeatures *>(link))
                     ->bufferDeviceAddressCaptureReplay);
            break;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT:
            hide(const_cast<VkPhysicalDeviceBufferDeviceAddressFeaturesEXT *>(
                     reinterpret_cast<const VkPhysicalDeviceBufferDeviceAddressFeaturesEXT *>(link))
                     ->bufferDeviceAddressCaptureReplay);
            break;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR:
        {
            auto *structure = const_cast<VkPhysicalDeviceAccelerationStructureFeaturesKHR *>(
                reinterpret_cast<const VkPhysicalDeviceAccelerationStructureFeaturesKHR *>(link));
            hide(structure->accelerationStructureCaptureReplay);
            hide(structure->accelerationStructureHostCommands);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR: {
            auto *features = const_cast<VkPhysicalDeviceRayTracingPipelineFeaturesKHR *>(
                reinterpret_cast<const VkPhysicalDeviceRayTracingPipelineFeaturesKHR *>(link));
            hide(features->rayTracingPipelineShaderGroupHandleCaptureReplay);
            hide(features->rayTracingPipelineShaderGroupHandleCaptureReplayMixed);
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT:
            hide(const_cast<VkPhysicalDeviceDescriptorBufferFeaturesEXT *>(
                     reinterpret_cast<const VkPhysicalDeviceDescriptorBufferFeaturesEXT *>(link))
                     ->descriptorBufferCaptureReplay);
            break;
        default: break;
        }
    }
}

uint32_t VulkanRobustAccess::hidden() const
{
    AV_SCOPE("VulkanRobustAccess::hidden");
    return hidden_;
}

void VulkanRobustAccess::hide(VkBool32 &feature)
{
    AV_SCOPE("VulkanRobustAccess::hide");
    if (feature == VK_FALSE) return;
    feature = VK_FALSE;
    hidden_++;
}

bool VulkanRobustAccess::apply(VkPhysicalDevice physical, VkDeviceCreateInfo &create)
{
    AV_SCOPE("VulkanRobustAccess::apply");
    hide_capture_replay(create.pNext);
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
