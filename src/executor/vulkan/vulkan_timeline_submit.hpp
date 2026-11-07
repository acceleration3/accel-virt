#ifndef AV_VULKAN_TIMELINE_SUBMIT_HPP
#define AV_VULKAN_TIMELINE_SUBMIT_HPP

#include "core/platform/block_source.hpp"
#include "core/support/growable_array.hpp"

#include <vulkan/vulkan.h>

#include <stdint.h>

namespace av {

class VulkanTimelineSubmit {
public:
    explicit VulkanTimelineSubmit(BlockSource &blocks);
    VulkanTimelineSubmit(const VulkanTimelineSubmit &) = delete;
    VulkanTimelineSubmit &operator=(const VulkanTimelineSubmit &) = delete;

    const VkSubmitInfo  *append(const VkSubmitInfo *submits, uint32_t count, VkSemaphore semaphore, uint64_t value,
                                uint32_t &appended);
    const VkSubmitInfo2 *append(const VkSubmitInfo2 *submits, uint32_t count, VkSemaphore semaphore, uint64_t value,
                                uint32_t &appended);

private:
    bool signalled(const VkSubmitInfo &last, VkSemaphore semaphore, uint64_t value);

    GrowableArray<VkSubmitInfo>          submits_;
    GrowableArray<VkSubmitInfo2>         submits2_;
    GrowableArray<VkSemaphore>           semaphores_;
    GrowableArray<uint64_t>              values_;
    GrowableArray<uint64_t>              waits_;
    GrowableArray<VkSemaphoreSubmitInfo> infos_;
    VkTimelineSemaphoreSubmitInfo        timeline_;
};

}

#endif
