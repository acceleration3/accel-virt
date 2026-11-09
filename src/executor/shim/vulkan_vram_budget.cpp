#include "executor/shim/vulkan_vram_budget.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

VulkanVramBudget::VulkanVramBudget() : physical_(VK_NULL_HANDLE)
{
    AV_SCOPE("VulkanVramBudget::VulkanVramBudget");
}

void VulkanVramBudget::bind(VkPhysicalDevice physical)
{
    AV_SCOPE("VulkanVramBudget::bind");
    physical_ = physical;
}

bool VulkanVramBudget::device_local(uint64_t &budget, uint64_t &usage)
{
    AV_SCOPE("VulkanVramBudget::device_local");
    if (physical_ == VK_NULL_HANDLE) return false;
    VkPhysicalDeviceMemoryBudgetPropertiesEXT reported = {};
    reported.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    VkPhysicalDeviceMemoryProperties2 properties = {};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    properties.pNext = &reported;
    ::vkGetPhysicalDeviceMemoryProperties2(physical_, &properties);
    budget = 0u;
    usage = 0u;
    for (uint32_t heap = 0u; heap < properties.memoryProperties.memoryHeapCount; heap++) {
        if ((properties.memoryProperties.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0u) continue;
        budget += reported.heapBudget[heap];
        usage += reported.heapUsage[heap];
    }
    return budget != 0u;
}

}
