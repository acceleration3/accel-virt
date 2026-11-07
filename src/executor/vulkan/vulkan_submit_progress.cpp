#include "executor/vulkan/vulkan_submit_progress.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

VulkanSubmitProgress::VulkanSubmitProgress() : slots_{}, marked_(0u)
{
    AV_SCOPE("VulkanSubmitProgress::VulkanSubmitProgress");
}

uint64_t VulkanSubmitProgress::marked() const
{
    AV_SCOPE("VulkanSubmitProgress::marked");
    return marked_;
}

void VulkanSubmitProgress::settle()
{
    AV_SCOPE("VulkanSubmitProgress::settle");
    for (Slot &slot : slots_) {
        if (!slot.pending_ || ::vkGetFenceStatus(slot.device_, slot.fence_) != VK_SUCCESS) continue;
        static_cast<void>(::vkResetFences(slot.device_, 1u, &slot.fence_));
        slot.pending_ = false;
    }
}

uint64_t VulkanSubmitProgress::reached()
{
    AV_SCOPE("VulkanSubmitProgress::reached");
    settle();
    uint64_t reached = marked_;
    for (const Slot &slot : slots_)
        if (slot.pending_ && slot.mark_ - 1u < reached) reached = slot.mark_ - 1u;
    return reached;
}

uint32_t VulkanSubmitProgress::oldest() const
{
    AV_SCOPE("VulkanSubmitProgress::oldest");
    uint32_t found = slots;
    for (uint32_t index = 0u; index < slots; index++)
        if (slots_[index].pending_ && (found == slots || slots_[index].mark_ < slots_[found].mark_)) found = index;
    return found;
}

uint32_t VulkanSubmitProgress::vacant()
{
    AV_SCOPE("VulkanSubmitProgress::vacant");
    settle();
    for (uint32_t index = 0u; index < slots; index++)
        if (!slots_[index].pending_) return index;
    const uint32_t waiting = oldest();
    Slot          &slot = slots_[waiting];
    if (::vkWaitForFences(slot.device_, 1u, &slot.fence_, VK_TRUE, wait_ns) == VK_SUCCESS) {
        static_cast<void>(::vkResetFences(slot.device_, 1u, &slot.fence_));
        slot.pending_ = false;
        return waiting;
    }
    return slots;
}

uint64_t VulkanSubmitProgress::mark(uint64_t device, uint64_t queue)
{
    AV_SCOPE("VulkanSubmitProgress::mark");
    const uint32_t at = vacant();
    if (at == slots) return 0u;
    Slot          &slot = slots_[at];
    const VkDevice owner = reinterpret_cast<VkDevice>(device);
    if (slot.fence_ == VK_NULL_HANDLE || slot.device_ != owner) {
        if (slot.fence_ != VK_NULL_HANDLE) ::vkDestroyFence(slot.device_, slot.fence_, nullptr);
        VkFenceCreateInfo create = {};
        create.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        slot.fence_ = VK_NULL_HANDLE;
        if (::vkCreateFence(owner, &create, nullptr, &slot.fence_) != VK_SUCCESS) return 0u;
        slot.device_ = owner;
    }
    if (::vkQueueSubmit(reinterpret_cast<VkQueue>(queue), 0u, nullptr, slot.fence_) != VK_SUCCESS) return 0u;
    slot.mark_ = ++marked_;
    slot.pending_ = true;
    return slot.mark_;
}

void VulkanSubmitProgress::wait_idle()
{
    AV_SCOPE("VulkanSubmitProgress::wait_idle");
    for (Slot &slot : slots_) {
        if (!slot.pending_) continue;
        if (::vkWaitForFences(slot.device_, 1u, &slot.fence_, VK_TRUE, wait_ns) != VK_SUCCESS) continue;
        static_cast<void>(::vkResetFences(slot.device_, 1u, &slot.fence_));
        slot.pending_ = false;
    }
}

void VulkanSubmitProgress::forget(VkDevice device)
{
    AV_SCOPE("VulkanSubmitProgress::forget");
    for (Slot &slot : slots_) {
        if (slot.fence_ == VK_NULL_HANDLE || slot.device_ != device) continue;
        if (slot.pending_) static_cast<void>(::vkWaitForFences(device, 1u, &slot.fence_, VK_TRUE, wait_ns));
        ::vkDestroyFence(device, slot.fence_, nullptr);
        slot = Slot{};
    }
}

bool VulkanSubmitProgress::invariants_hold() const
{
    AV_SCOPE("VulkanSubmitProgress::invariants_hold");
    for (const Slot &slot : slots_)
        if (slot.pending_ && (slot.fence_ == VK_NULL_HANDLE || slot.mark_ == 0u || slot.mark_ > marked_)) return false;
    return true;
}

}
