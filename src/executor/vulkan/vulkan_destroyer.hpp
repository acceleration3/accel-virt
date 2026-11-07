#ifndef AV_VULKAN_DESTROYER_HPP
#define AV_VULKAN_DESTROYER_HPP

#include "executor/vulkan/vulkan_destroy_kind.hpp"

#include <stdint.h>

namespace av {

class VulkanDestroyer {
public:
    virtual void destroy(VulkanDestroyKind kind, uint64_t device, uint64_t parent, uint64_t handle) = 0;

protected:
    VulkanDestroyer() = default;
    ~VulkanDestroyer() = default;
};

}

#endif
