#include "executor/vulkan/vulkan_backend.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/vulkan/fence_wake.hpp"

namespace av {

VkResult VulkanBackend::on_vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence *pFences,
                                           VkBool32 waitAll, uint64_t timeout)
{
    AV_SCOPE("VulkanBackend::on_vkWaitForFences");
    if (timeout != vkproto::FenceWake::arm_timeout)
        return RealVulkanDispatch::on_vkWaitForFences(device, fenceCount, pFences, waitAll, timeout);
    const VkResult asked = RealVulkanDispatch::on_vkWaitForFences(device, fenceCount, pFences, waitAll, 0u);
    if (asked != VK_TIMEOUT) return asked;
    return fence_watch_.arm(device, pFences, fenceCount, waitAll) ? VK_TIMEOUT : VK_NOT_READY;
}

void VulkanBackend::on_vkDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks *)
{
    AV_SCOPE("VulkanBackend::on_vkDestroyFence");
    if (fence_watch_.armed() != 0u) fence_watch_.settle();
    RealVulkanDispatch::on_vkDestroyFence(device, fence, nullptr);
}

VkResult VulkanBackend::on_vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence *pFences)
{
    AV_SCOPE("VulkanBackend::on_vkResetFences");
    if (fence_watch_.armed() != 0u) fence_watch_.settle();
    return RealVulkanDispatch::on_vkResetFences(device, fenceCount, pFences);
}

}
