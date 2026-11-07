#ifndef AV_VULKAN_FENCE_TARGETS_HPP
#define AV_VULKAN_FENCE_TARGETS_HPP

#include "core/platform/block_source.hpp"
#include "core/support/keyed_slots.hpp"

#include <vulkan/vulkan.h>

#include <stdint.h>

namespace av {

class VulkanFenceTargets {
public:
    struct Target {
        VkSemaphore semaphore_;
        uint64_t    value_;
    };

    explicit VulkanFenceTargets(BlockSource &blocks);
    VulkanFenceTargets(const VulkanFenceTargets &) = delete;
    VulkanFenceTargets &operator=(const VulkanFenceTargets &) = delete;

    void     note(VkFence fence, VkSemaphore semaphore, uint64_t value);
    void     forget(VkFence fence);
    void     clear();
    bool     find(VkFence fence, Target &target) const;
    uint32_t count() const;

private:
    KeyedSlots<uint64_t, Target> targets_;
};

}

#endif
