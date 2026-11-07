#ifndef AV_LOW_PRIORITY_QUEUES_HPP
#define AV_LOW_PRIORITY_QUEUES_HPP

#include <stdint.h>

#include <vulkan/vulkan.h>

namespace av {

class LowPriorityQueues {
public:
    static constexpr uint32_t most_queue_infos = 16u;

    LowPriorityQueues();
    LowPriorityQueues(const LowPriorityQueues &) = delete;
    LowPriorityQueues &operator=(const LowPriorityQueues &) = delete;

    bool        offered_by(VkPhysicalDevice physical);
    const char *extension() const;
    bool        apply(VkDeviceCreateInfo &create);

    bool invariants_hold() const;

private:
    VkDeviceQueueCreateInfo                 infos_[most_queue_infos];
    VkDeviceQueueGlobalPriorityCreateInfoKHR priorities_[most_queue_infos];
    const char                              *extension_;
};

}

#endif
