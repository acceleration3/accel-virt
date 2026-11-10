#include "proto/vulkan/placed_memory_types.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

uint32_t PlacedMemoryTypes::mask(const VkPhysicalDeviceMemoryProperties &properties)
{
    AV_SCOPE("PlacedMemoryTypes::mask");
    uint32_t placed = 0u;
    const uint32_t count = properties.memoryTypeCount < VK_MAX_MEMORY_TYPES ? properties.memoryTypeCount : VK_MAX_MEMORY_TYPES;
    for (uint32_t type = 0u; type < count; type++) {
        const VkMemoryType &kind = properties.memoryTypes[type];
        if ((kind.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0u) continue;
        for (uint32_t other = 0u; other < count; other++) {
            if (other == type) continue;
            const VkMemoryType &twin = properties.memoryTypes[other];
            if (twin.heapIndex == kind.heapIndex && twin.propertyFlags == kind.propertyFlags) {
                placed |= 1u << type;
                break;
            }
        }
    }
    return placed;
}

}
