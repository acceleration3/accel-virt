#include "icd/vulkan/fence_ready.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/vulkan/entry_lookup.hpp"

namespace av::icd {

namespace {

using WaitFences = VkResult(VKAPI_PTR *)(VkDevice, uint32_t, const VkFence *, VkBool32, uint64_t);
using ResetFences = VkResult(VKAPI_PTR *)(VkDevice, uint32_t, const VkFence *);

}

FenceReady::FenceReady(EntryLookup &entries) : entries_(entries)
{
    AV_SCOPE("FenceReady::FenceReady");
}

FrameWait FenceReady::wait(const Mark &mark)
{
    AV_SCOPE("FenceReady::wait");
    const auto wait_fences = reinterpret_cast<WaitFences>(entries_.entry("vkWaitForFences"));
    const auto reset_fences = reinterpret_cast<ResetFences>(entries_.entry("vkResetFences"));
    if (wait_fences == nullptr || reset_fences == nullptr || mark.fence_ == VK_NULL_HANDLE) return FrameWait::gone;
    const VkResult waited = wait_fences(mark.device_, 1u, &mark.fence_, VK_TRUE, slice_ns);
    if (waited == VK_TIMEOUT) return FrameWait::later;
    if (waited != VK_SUCCESS) return FrameWait::gone;
    return reset_fences(mark.device_, 1u, &mark.fence_) == VK_SUCCESS ? FrameWait::drawn : FrameWait::gone;
}

}
