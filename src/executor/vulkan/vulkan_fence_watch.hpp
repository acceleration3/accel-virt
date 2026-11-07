#ifndef AV_VULKAN_FENCE_WATCH_HPP
#define AV_VULKAN_FENCE_WATCH_HPP

#include "executor/wake_signal.hpp"

#include <pthread.h>
#include <stdint.h>

#include <vulkan/vulkan.h>

namespace av {

class VulkanFenceWatch {
public:
    static constexpr uint32_t most_watches = 8u;
    static constexpr uint32_t most_fences = 16u;
    static constexpr uint64_t slice_ns = 5000000u;

    VulkanFenceWatch();
    ~VulkanFenceWatch();
    VulkanFenceWatch(const VulkanFenceWatch &) = delete;
    VulkanFenceWatch &operator=(const VulkanFenceWatch &) = delete;

    void               bind(void (*signal)(void *context), void *context);
    void               unbind();
    bool               bound() const;
    struct Target {
        VkSemaphore semaphore_;
        uint64_t    value_;
    };

    [[nodiscard]] bool arm(VkDevice device, const VkFence *fences, uint32_t count, VkBool32 wait_all,
                           const Target *targets = nullptr);
    void               settle();

    static constexpr uint32_t idle = 0u;
    static constexpr uint32_t running = 1u;
    static constexpr uint32_t finished = 2u;

    struct Watch {
        VulkanFenceWatch *owner_;
        pthread_t         thread_;
        VkDevice          device_;
        VkFence           fences_[most_fences];
        uint32_t          count_;
        VkBool32          all_;
        Target            targets_[most_fences];
        VkSemaphore       cancel_;
        uint64_t          cancel_at_;
        bool              targeted_;
        uint32_t          state_;
    };

    void run(Watch &watch);

    uint32_t armed() const;
    uint64_t raised() const;

    bool invariants_hold() const;

    void forget(VkDevice device);

private:
    static constexpr uint32_t most_cancels = 4u;

    struct Cancel {
        VkDevice    device_;
        VkSemaphore semaphore_;
        uint64_t    next_;
    };

    Cancel  *cancel_for(VkDevice device);
    void     wait_timelines(Watch &watch);
    uint32_t vacancy();
    bool     stopping() const;

    WakeSignal signal_;
    Watch      watches_[most_watches];
    Cancel     cancels_[most_cancels];
    uint32_t   stopping_;
    uint64_t   raised_;
};

}

#endif
