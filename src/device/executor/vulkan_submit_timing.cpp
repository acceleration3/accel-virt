#include "device/executor/vulkan_submit_timing.hpp"

#include "core/trace/function_scope.hpp"

#include <stdio.h>
#include <time.h>

extern "C" char *getenv(const char *name);   /* freestanding: libc's own */

namespace av {

namespace {

constexpr uint64_t nanoseconds_per_second = 1000000000u;

}  // namespace

VulkanSubmitTiming::VulkanSubmitTiming()
    : enabled_(getenv("AV_VK_SUBMIT_TIMING") != nullptr), device_(VK_NULL_HANDLE), queue_(VK_NULL_HANDLE),
      pool_(VK_NULL_HANDLE), queries_(VK_NULL_HANDLE), begin_{}, end_{}, pending_{}, next_(0u), oldest_(0u),
      period_ns_(1.0), last_end_(0u), busy_ns_(0u), gap_ns_(0u), gaps_(0u), long_gaps_(0u), timed_(0u), window_started_(0u)
{
    AV_SCOPE("VulkanSubmitTiming::VulkanSubmitTiming");
}

VulkanSubmitTiming::~VulkanSubmitTiming()
{
    AV_SCOPE("VulkanSubmitTiming::~VulkanSubmitTiming");
    if (device_ != VK_NULL_HANDLE) forget(device_);
}

bool VulkanSubmitTiming::enabled() const
{
    AV_SCOPE("VulkanSubmitTiming::enabled");
    return enabled_;
}

void VulkanSubmitTiming::queue_made(VkPhysicalDevice physical, VkDevice device, uint32_t family, VkQueue queue)
{
    AV_SCOPE("VulkanSubmitTiming::queue_made");
    if (!enabled_ || queue_ != VK_NULL_HANDLE || queue == VK_NULL_HANDLE) return;
    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(physical, &properties);
    period_ns_ = static_cast<double>(properties.limits.timestampPeriod);
    VkCommandPoolCreateInfo pool = {};
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = family;
    if (vkCreateCommandPool(device, &pool, nullptr, &pool_) != VK_SUCCESS) return;
    VkQueryPoolCreateInfo queries = {};
    queries.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queries.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queries.queryCount = 2u * slots;
    if (vkCreateQueryPool(device, &queries, nullptr, &queries_) != VK_SUCCESS) return;
    VkCommandBufferAllocateInfo buffers = {};
    buffers.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    buffers.commandPool = pool_;
    buffers.commandBufferCount = slots;
    if (vkAllocateCommandBuffers(device, &buffers, begin_) != VK_SUCCESS ||
        vkAllocateCommandBuffers(device, &buffers, end_) != VK_SUCCESS)
        return;
    device_ = device;
    queue_ = queue;
    window_started_ = now();
    fprintf(stderr, "av-vk-timing: timing queue family %u\n", family);
}

void VulkanSubmitTiming::harvest()
{
    AV_SCOPE("VulkanSubmitTiming::harvest");
    while (pending_[oldest_]) {
        uint64_t results[4] = {};
        const VkResult read = vkGetQueryPoolResults(device_, queries_, 2u * oldest_, 2u, sizeof results, results,
                                                    2u * sizeof(uint64_t),
                                                    VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
        if (read != VK_SUCCESS || results[1] == 0u || results[3] == 0u) return;
        busy_ns_ += static_cast<uint64_t>(static_cast<double>(results[2] - results[0]) * period_ns_);
        /* A gap past a millisecond is the application doing something else -
         * loading, or not submitting at all - and is counted apart. */
        constexpr uint64_t long_gap_ns = 1000000u;
        if (last_end_ != 0u && results[0] > last_end_) {
            const uint64_t gap = static_cast<uint64_t>(static_cast<double>(results[0] - last_end_) * period_ns_);
            if (gap < long_gap_ns) {
                gap_ns_ += gap;
                gaps_++;
            } else {
                long_gaps_++;
            }
        }
        last_end_ = results[2];
        timed_++;
        pending_[oldest_] = false;
        oldest_ = (oldest_ + 1u) % slots;
    }
}

uint64_t VulkanSubmitTiming::now() const
{
    AV_SCOPE("VulkanSubmitTiming::now");
    timespec clock = {};
    clock_gettime(CLOCK_MONOTONIC, &clock);
    return static_cast<uint64_t>(clock.tv_sec) * nanoseconds_per_second + static_cast<uint64_t>(clock.tv_nsec);
}

void VulkanSubmitTiming::report()
{
    AV_SCOPE("VulkanSubmitTiming::report");
    const uint64_t at = now();
    if (at - window_started_ < nanoseconds_per_second || timed_ == 0u) return;
    const double seconds = static_cast<double>(at - window_started_) / 1e9;
    fprintf(stderr,
            "av-vk-timing: %.0f submits/s, GPU %.1f us in each, %.1f us idle before each (of %llu; %llu idle over "
            "1 ms), in submits %.1f%% of the time\n",
            static_cast<double>(timed_) / seconds, static_cast<double>(busy_ns_) / 1e3 / static_cast<double>(timed_),
            gaps_ == 0u ? 0.0 : static_cast<double>(gap_ns_) / 1e3 / static_cast<double>(gaps_),
            static_cast<unsigned long long>(gaps_), static_cast<unsigned long long>(long_gaps_),
            100.0 * static_cast<double>(busy_ns_) / static_cast<double>(at - window_started_));
    busy_ns_ = gap_ns_ = timed_ = gaps_ = long_gaps_ = 0u;
    window_started_ = at;
}

VkResult VulkanSubmitTiming::submit(VkQueue queue, uint32_t count, const VkSubmitInfo *submits, VkFence fence)
{
    AV_SCOPE("VulkanSubmitTiming::submit");
    if (queue != queue_ || count > most_submits) return vkQueueSubmit(queue, count, submits, fence);
    harvest();
    report();
    const uint32_t slot = next_;
    uint32_t       buffers = 2u;
    for (uint32_t index = 0u; index < count; index++) buffers += submits[index].commandBufferCount;
    if (pending_[slot] || buffers > most_buffers) return vkQueueSubmit(queue, count, submits, fence);
    VkCommandBufferBeginInfo begin = {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(begin_[slot], &begin);
    vkCmdResetQueryPool(begin_[slot], queries_, 2u * slot, 2u);
    vkCmdWriteTimestamp(begin_[slot], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queries_, 2u * slot);
    vkEndCommandBuffer(begin_[slot]);
    vkBeginCommandBuffer(end_[slot], &begin);
    vkCmdWriteTimestamp(end_[slot], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries_, 2u * slot + 1u);
    vkEndCommandBuffer(end_[slot]);
    VkSubmitInfo    infos[most_submits] = {};
    VkCommandBuffer lists[most_buffers] = {};
    uint32_t        used = 0u;
    const uint32_t  made = count == 0u ? 1u : count;
    for (uint32_t index = 0u; index < made; index++) {
        if (count != 0u) {
            infos[index] = submits[index];
        } else {
            infos[index] = VkSubmitInfo{};
            infos[index].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        }
        VkCommandBuffer *const list = lists + used;
        uint32_t              listed = 0u;
        if (index == 0u) list[listed++] = begin_[slot];
        for (uint32_t buffer = 0u; count != 0u && buffer < submits[index].commandBufferCount; buffer++)
            list[listed++] = submits[index].pCommandBuffers[buffer];
        if (index == made - 1u) list[listed++] = end_[slot];
        infos[index].commandBufferCount = listed;
        infos[index].pCommandBuffers = list;
        used += listed;
    }
    const VkResult submitted = vkQueueSubmit(queue, made, infos, fence);
    if (submitted == VK_SUCCESS) {
        pending_[slot] = true;
        next_ = (next_ + 1u) % slots;
    }
    return submitted;
}

void VulkanSubmitTiming::forget(VkDevice device)
{
    AV_SCOPE("VulkanSubmitTiming::forget");
    if (device != device_ || device_ == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(device_);
    if (queries_ != VK_NULL_HANDLE) vkDestroyQueryPool(device_, queries_, nullptr);
    if (pool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, pool_, nullptr);
    device_ = VK_NULL_HANDLE;
    queue_ = VK_NULL_HANDLE;
    queries_ = VK_NULL_HANDLE;
    pool_ = VK_NULL_HANDLE;
    for (bool &pending : pending_) pending = false;
    next_ = oldest_ = 0u;
    last_end_ = 0u;
}

}  // namespace av
