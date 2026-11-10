#ifndef AV_PLACED_MEMORY_TYPES_HPP
#define AV_PLACED_MEMORY_TYPES_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {

class PlacedMemoryTypes {
public:
    static uint32_t mask(const VkPhysicalDeviceMemoryProperties &properties);
};

}

#endif
