#ifndef AV_FAKE_VULKAN_DRIVER_HPP
#define AV_FAKE_VULKAN_DRIVER_HPP

#include <vulkan/vulkan_core.h>

#include <cstdint>

namespace av {

struct FakeVulkanDriver {
    static constexpr std::uint32_t most_events = 16u;
    static constexpr std::uint32_t most_calls = 64u;
    static constexpr std::uint32_t most_fences = 64u;
    static constexpr std::uint32_t most_families = 8u;

    std::uint32_t families = 0u;
    VkQueueFlags  family_flags[most_families] = {};

    std::uint64_t next_handle = 0x5000u;
    std::uint32_t pools_made = 0u;
    std::uint32_t pools_destroyed = 0u;
    std::uint32_t fills = 0u;
    std::uint32_t draws = 0u;
    std::uint32_t dispatches = 0u;
    std::uint32_t query_resets = 0u;
    std::uint32_t waits_recorded = 0u;
    std::uint32_t wait_events_seen = 0u;

    VkResult      event_status[most_events] = {};
    std::uint32_t events_set = 0u;
    std::uint32_t event_polls = 0u;
    VkResult      set_result = VK_EVENT_SET;

    std::uint32_t submits = 0u;
    VkQueue       submitted_queue[most_calls] = {};
    VkFence       submitted_fence[most_calls] = {};
    VkCommandBuffer submitted_buffer[most_calls] = {};
    std::uint32_t submitted_buffers = 0u;
    VkResult      submit_result = VK_SUCCESS;

    VkFenceCreateInfo unused = {};
    bool          fence_signalled[most_fences] = {};
    std::uint32_t fences_made = 0u;
    std::uint32_t fences_destroyed = 0u;
    std::uint32_t idles = 0u;
    std::uint32_t device_destroys = 0u;
    VkResult      idle_result = VK_SUCCESS;
    VkResult      fence_status = VK_NOT_READY;
};

inline FakeVulkanDriver fake_vulkan;

inline std::uint64_t fake_handle()
{
    fake_vulkan.next_handle += 0x100u;
    return fake_vulkan.next_handle;
}

inline std::uint32_t fake_event_index(VkEvent event)
{
    return static_cast<std::uint32_t>((reinterpret_cast<std::uintptr_t>(event) >> 8u) & 0xFu);
}

}

extern "C" {

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice, uint32_t *count,
                                                                           VkQueueFamilyProperties *properties)
{
    const av::FakeVulkanDriver &driver = av::fake_vulkan;
    if (properties == nullptr) {
        *count = driver.families;
        return;
    }
    if (*count > driver.families) *count = driver.families;
    for (uint32_t family = 0u; family < *count; family++) {
        properties[family] = {};
        properties[family].queueFlags = driver.family_flags[family];
        properties[family].queueCount = 1u;
    }
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateCommandPool(VkDevice, const VkCommandPoolCreateInfo *,
                                                          const VkAllocationCallbacks *, VkCommandPool *pool)
{
    av::fake_vulkan.pools_made++;
    *pool = reinterpret_cast<VkCommandPool>(av::fake_handle());
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyCommandPool(VkDevice, VkCommandPool, const VkAllocationCallbacks *)
{
    av::fake_vulkan.pools_destroyed++;
}

VKAPI_ATTR void VKAPI_CALL vkCmdFillBuffer(VkCommandBuffer, VkBuffer, VkDeviceSize, VkDeviceSize, uint32_t)
{
    av::fake_vulkan.fills++;
}

VKAPI_ATTR void VKAPI_CALL vkCmdDraw(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t)
{
    av::fake_vulkan.draws++;
}

VKAPI_ATTR void VKAPI_CALL vkCmdDispatch(VkCommandBuffer, uint32_t, uint32_t, uint32_t)
{
    av::fake_vulkan.dispatches++;
}

VKAPI_ATTR void VKAPI_CALL vkCmdResetQueryPool(VkCommandBuffer, VkQueryPool, uint32_t, uint32_t)
{
    av::fake_vulkan.query_resets++;
}

VKAPI_ATTR void VKAPI_CALL vkCmdWaitEvents(VkCommandBuffer, uint32_t, const VkEvent *, VkPipelineStageFlags,
                                           VkPipelineStageFlags, uint32_t, const VkMemoryBarrier *, uint32_t,
                                           const VkBufferMemoryBarrier *, uint32_t, const VkImageMemoryBarrier *)
{
    av::fake_vulkan.waits_recorded++;
}

VKAPI_ATTR void VKAPI_CALL vkCmdWaitEvents2(VkCommandBuffer, uint32_t, const VkEvent *, const VkDependencyInfo *)
{
    av::fake_vulkan.waits_recorded++;
}

VKAPI_ATTR VkResult VKAPI_CALL vkGetEventStatus(VkDevice, VkEvent event)
{
    av::fake_vulkan.event_polls++;
    return av::fake_vulkan.event_status[av::fake_event_index(event)];
}

VKAPI_ATTR VkResult VKAPI_CALL vkSetEvent(VkDevice, VkEvent event)
{
    av::fake_vulkan.events_set++;
    av::fake_vulkan.event_status[av::fake_event_index(event)] = VK_EVENT_SET;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateFence(VkDevice, const VkFenceCreateInfo *, const VkAllocationCallbacks *,
                                             VkFence *fence)
{
    av::fake_vulkan.fences_made++;
    *fence = reinterpret_cast<VkFence>(static_cast<std::uintptr_t>(av::fake_vulkan.fences_made));
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyFence(VkDevice, VkFence, const VkAllocationCallbacks *)
{
    av::fake_vulkan.fences_destroyed++;
}

VKAPI_ATTR VkResult VKAPI_CALL vkResetFences(VkDevice, uint32_t, const VkFence *)
{
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkGetFenceStatus(VkDevice, VkFence)
{
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(VkDevice, uint32_t, const VkFence *, VkBool32, uint64_t)
{
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo *)
{
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice)
{
    av::fake_vulkan.idles++;
    return av::fake_vulkan.idle_result;
}

VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue, uint32_t count, const VkSubmitInfo *submits, VkFence fence)
{
    av::FakeVulkanDriver &driver = av::fake_vulkan;
    if (count == 0u) return VK_SUCCESS;
    if (driver.submits < av::FakeVulkanDriver::most_calls) {
        driver.submitted_queue[driver.submits] = queue;
        driver.submitted_fence[driver.submits] = fence;
        driver.submitted_buffer[driver.submits] = submits[0].commandBufferCount != 0u ? submits[0].pCommandBuffers[0] : nullptr;
    }
    driver.submits++;
    return driver.submit_result;
}

}

#endif
