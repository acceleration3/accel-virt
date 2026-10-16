/* How long the GPU spends inside each guest submission, and idle between one
 * and the next, logged once a second: AV_VK_SUBMIT_TIMING=1 in QEMU's
 * environment. A timestamp is written before a submission's first command
 * buffer and after its last, from command buffers of this class's own; the
 * results are read back without waiting, submissions later. What a frame the
 * GPU takes longer over in the VM than natively spends inside the work and
 * what between it are the two numbers here. */
#ifndef AV_DEVICE_EXECUTOR_VULKAN_SUBMIT_TIMING_HPP
#define AV_DEVICE_EXECUTOR_VULKAN_SUBMIT_TIMING_HPP

#include <vulkan/vulkan.h>

#include <stdint.h>

namespace av {

class DriverLog;

class VulkanSubmitTiming {
public:
    /* `lines` is where the timing is said. */
    explicit VulkanSubmitTiming(DriverLog &lines);
    ~VulkanSubmitTiming();
    VulkanSubmitTiming(const VulkanSubmitTiming &) = delete;
    VulkanSubmitTiming &operator=(const VulkanSubmitTiming &) = delete;

    bool     enabled() const;
    /* `queue` is family `family` of `device`, whose physical device is `physical`. */
    void     queue_made(VkPhysicalDevice physical, VkDevice device, uint32_t family, VkQueue queue);
    /* The guest's submission, timed: its own submit infos with one command
     * buffer of this class's before the first and after the last. */
    VkResult submit(VkQueue queue, uint32_t count, const VkSubmitInfo *submits, VkFence fence);
    /* Before `device` goes. */
    void     forget(VkDevice device);

private:
    static constexpr uint32_t slots = 64u;
    static constexpr uint32_t most_submits = 16u;
    static constexpr uint32_t most_buffers = 64u;

    void     harvest();
    void     report();
    uint64_t now() const;

    DriverLog       &lines_;
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

}  // namespace av

#endif  // AV_DEVICE_EXECUTOR_VULKAN_SUBMIT_TIMING_HPP
