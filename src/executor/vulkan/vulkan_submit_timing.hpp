#ifndef AV_DEVICE_EXECUTOR_VULKAN_SUBMIT_TIMING_HPP
#define AV_DEVICE_EXECUTOR_VULKAN_SUBMIT_TIMING_HPP

#include "executor/monotonic_clock.hpp"

#include <vulkan/vulkan.h>

#include <stdint.h>

namespace av {

class DriverLog;

class VulkanSubmitTiming {
public:
    explicit VulkanSubmitTiming(DriverLog &lines);
    ~VulkanSubmitTiming();
    VulkanSubmitTiming(const VulkanSubmitTiming &) = delete;
    VulkanSubmitTiming &operator=(const VulkanSubmitTiming &) = delete;

    bool     enabled() const;
    void     queue_made(VkPhysicalDevice physical, VkDevice device, uint32_t family, VkQueue queue);
    VkResult submit(VkQueue queue, uint32_t count, const VkSubmitInfo *submits, VkFence fence);
    void     forget(VkDevice device);

private:
    static constexpr uint32_t slots = 64u;
    static constexpr uint32_t most_submits = 16u;
    static constexpr uint32_t most_buffers = 64u;

    void     harvest();
    void     report();

    DriverLog       &lines_;
    MonotonicClock   clock_;
    bool             enabled_;
    VkDevice         device_;
    VkQueue          queue_;
    VkCommandPool    pool_;
    VkQueryPool      queries_;
    VkCommandBuffer  begin_[slots];
    VkCommandBuffer  end_[slots];
    bool             pending_[slots];
    uint32_t         next_;
    uint32_t         oldest_;
    double           period_ns_;
    uint64_t         last_end_;
    uint64_t         busy_ns_;
    uint64_t         gap_ns_;
    uint64_t         gaps_;
    uint64_t         long_gaps_;
    uint64_t         timed_;
    uint64_t         window_started_;
};

}

#endif
