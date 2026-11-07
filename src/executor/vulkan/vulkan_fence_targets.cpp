#include "executor/vulkan/vulkan_fence_targets.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

VulkanFenceTargets::VulkanFenceTargets(BlockSource &blocks) : targets_(blocks, Pool::non_paged)
{
    AV_SCOPE("VulkanFenceTargets::VulkanFenceTargets");
}

void VulkanFenceTargets::note(VkFence fence, VkSemaphore semaphore, uint64_t value)
{
    AV_SCOPE("VulkanFenceTargets::note");
    const uint64_t key = reinterpret_cast<uint64_t>(fence);
    if (Target *const known = targets_.find(key)) {
        *known = Target{semaphore, value};
        return;
    }
    uint32_t slot = 0u;
    static_cast<void>(targets_.insert(key, Target{semaphore, value}, slot));
}

void VulkanFenceTargets::forget(VkFence fence)
{
    AV_SCOPE("VulkanFenceTargets::forget");
    static_cast<void>(targets_.erase(reinterpret_cast<uint64_t>(fence)));
}

void VulkanFenceTargets::clear()
{
    AV_SCOPE("VulkanFenceTargets::clear");
    targets_.clear();
}

bool VulkanFenceTargets::find(VkFence fence, Target &target) const
{
    AV_SCOPE("VulkanFenceTargets::find");
    const Target *const known = targets_.find(reinterpret_cast<uint64_t>(fence));
    if (known == nullptr) return false;
    target = *known;
    return true;
}

uint32_t VulkanFenceTargets::count() const
{
    AV_SCOPE("VulkanFenceTargets::count");
    return targets_.count();
}

}
