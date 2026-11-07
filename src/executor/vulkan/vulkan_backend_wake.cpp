#include "executor/vulkan/vulkan_backend.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/vulkan/fence_wake.hpp"

namespace av {

VkResult VulkanBackend::on_vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence *pFences,
                                           VkBool32 waitAll, uint64_t timeout)
{
    AV_SCOPE("VulkanBackend::on_vkWaitForFences");
    if (!vkproto::FenceWake().arms(timeout))
        return RealVulkanDispatch::on_vkWaitForFences(device, fenceCount, pFences, waitAll, timeout);
    const VkResult asked = RealVulkanDispatch::on_vkWaitForFences(device, fenceCount, pFences, waitAll, 0u);
    if (asked != VK_TIMEOUT) return asked;
    VulkanFenceWatch::Target targets[VulkanFenceWatch::most_fences] = {};
    bool                     mapped = fenceCount <= VulkanFenceWatch::most_fences;
    for (uint32_t at = 0u; mapped && at < fenceCount; at++) {
        VulkanFenceTargets::Target found = {};
        mapped = fence_targets_.find(pFences[at], found);
        targets[at] = VulkanFenceWatch::Target{found.semaphore_, found.value_};
    }
    return fence_watch_.arm(device, pFences, fenceCount, waitAll, mapped ? targets : nullptr) ? VK_TIMEOUT : VK_NOT_READY;
}

void VulkanBackend::on_vkDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyFence");
    if (fence_watch_.armed() != 0u) fence_watch_.settle();
    fence_targets_.forget(fence);
    RealVulkanDispatch::on_vkDestroyFence(device, fence, nullptr);
}

VkResult VulkanBackend::on_vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence *pFences)
{
    AV_SCOPE("VulkanBackend::on_vkResetFences");
    if (fence_watch_.armed() != 0u) fence_watch_.settle();
    for (uint32_t at = 0u; at < fenceCount; at++) fence_targets_.forget(pFences[at]);
    return RealVulkanDispatch::on_vkResetFences(device, fenceCount, pFences);
}

}
