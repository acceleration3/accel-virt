#ifndef AV_VULKAN_QUEUE_TIMELINES_HPP
#define AV_VULKAN_QUEUE_TIMELINES_HPP

#include "core/platform/block_source.hpp"
#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <vulkan/vulkan.h>

#include <stdint.h>

namespace av {

class VulkanQueueTimelines {
public:
    explicit VulkanQueueTimelines(BlockSource &blocks);
    VulkanQueueTimelines(const VulkanQueueTimelines &) = delete;
    VulkanQueueTimelines &operator=(const VulkanQueueTimelines &) = delete;

    [[nodiscard]] Status enable(VkDevice device);
    bool                 enabled(uint64_t device) const;
    bool                 acquire(uint64_t device, uint64_t queue, VkSemaphore &semaphore, uint64_t &value);
    [[nodiscard]] Status open(uint64_t mark, uint64_t device);
    void                 close(uint64_t mark);
    bool                 commit(uint64_t mark, uint64_t queue, uint64_t value);
    void                 poll();
    uint64_t             floor(uint64_t device, uint64_t ceiling) const;
    bool                 settle_device(uint64_t device, uint64_t nanoseconds);
    bool                 settle_marks(uint64_t device, const uint64_t *marks, uint32_t count, uint64_t nanoseconds);
    void                 forget(VkDevice device);
    bool                 owns(uint64_t device) const;
    bool                 lost() const;
    uint32_t             lines() const;
    uint32_t             outstanding() const;

    bool invariants_hold() const;

private:
    struct Line {
        VkDevice    device_;
        VkQueue     queue_;
        VkSemaphore semaphore_;
        uint64_t    issued_;
        uint64_t    reached_;
    };

    struct Open {
        uint64_t mark_;
        uint64_t device_;
    };

    struct Pending {
        uint32_t line_;
        uint64_t mark_;
        uint64_t value_;
    };

    Line    *line_of(uint64_t queue);
    bool     waited(VkDevice device, const VkSemaphore *semaphores, const uint64_t *values, uint32_t count,
                    uint64_t nanoseconds);
    bool     open_for(uint64_t device) const;
    void     note_status(VkResult status);

    GrowableArray<VkDevice> devices_;
    GrowableArray<Line>     lines_;
    GrowableArray<Open>     open_;
    GrowableArray<Pending>  pending_;
    bool                    lost_;
};

}

#endif
