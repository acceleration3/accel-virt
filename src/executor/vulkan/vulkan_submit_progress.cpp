#include "executor/vulkan/vulkan_submit_progress.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/teardown_budget.hpp"

namespace av {

namespace {

uint64_t number(const void *handle)
{
    AV_SCOPE("number");
    return reinterpret_cast<uint64_t>(handle);
}

}

VulkanSubmitProgress::VulkanSubmitProgress() : slots_{}, marked_(0u), stale_(true)
{
    AV_SCOPE("VulkanSubmitProgress::VulkanSubmitProgress");
}

uint64_t VulkanSubmitProgress::marked() const
{
    AV_SCOPE("VulkanSubmitProgress::marked");
    return marked_;
}

void VulkanSubmitProgress::begin_poll()
{
    AV_SCOPE("VulkanSubmitProgress::begin_poll");
    stale_ = true;
}

void VulkanSubmitProgress::poll_all()
{
    AV_SCOPE("VulkanSubmitProgress::poll_all");
    for (Slot &slot : slots_) {
        if (slot.state_ == State::stuck) static_cast<void>(queued(slot));
        if (slot.state_ != State::pending) continue;
        const VkResult status = ::vkGetFenceStatus(slot.device_, slot.fence_);
        if (status == VK_SUCCESS) static_cast<void>(::vkResetFences(slot.device_, 1u, &slot.fence_));
        if (status == VK_SUCCESS || status == VK_ERROR_DEVICE_LOST) slot.state_ = State::vacant;
    }
}

void VulkanSubmitProgress::settle()
{
    AV_SCOPE("VulkanSubmitProgress::settle");
    if (!stale_) return;
    stale_ = false;
    poll_all();
}

uint64_t VulkanSubmitProgress::reached(uint64_t device)
{
    AV_SCOPE("VulkanSubmitProgress::reached");
    settle();
    uint64_t reached = marked_;
    for (const Slot &slot : slots_)
        if (slot.state_ != State::vacant && number(slot.device_) == device && slot.mark_ - 1u < reached)
            reached = slot.mark_ - 1u;
    return reached;
}

uint32_t VulkanSubmitProgress::oldest() const
{
    AV_SCOPE("VulkanSubmitProgress::oldest");
    uint32_t found = slots;
    for (uint32_t index = 0u; index < slots; index++)
        if (slots_[index].state_ == State::pending && (found == slots || slots_[index].mark_ < slots_[found].mark_))
            found = index;
    return found;
}

bool VulkanSubmitProgress::waited(Slot &slot, uint64_t nanoseconds)
{
    AV_SCOPE("VulkanSubmitProgress::waited");
    const VkResult status = ::vkWaitForFences(slot.device_, 1u, &slot.fence_, VK_TRUE, nanoseconds);
    if (status == VK_SUCCESS) static_cast<void>(::vkResetFences(slot.device_, 1u, &slot.fence_));
    if (status != VK_SUCCESS && status != VK_ERROR_DEVICE_LOST) return false;
    slot.state_ = State::vacant;
    return true;
}

uint32_t VulkanSubmitProgress::vacancy()
{
    AV_SCOPE("VulkanSubmitProgress::vacancy");
    poll_all();
    stale_ = false;
    for (uint32_t index = 0u; index < slots; index++)
        if (slots_[index].state_ == State::vacant) return index;
    const uint32_t waiting = oldest();
    if (waiting == slots || !waited(slots_[waiting])) return slots;
    return waiting;
}

bool VulkanSubmitProgress::fenced(Slot &slot, VkDevice owner)
{
    AV_SCOPE("VulkanSubmitProgress::fenced");
    if (slot.fence_ != VK_NULL_HANDLE && slot.device_ == owner) return true;
    if (slot.fence_ != VK_NULL_HANDLE) ::vkDestroyFence(slot.device_, slot.fence_, nullptr);
    VkFenceCreateInfo create = {};
    create.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    slot.fence_ = VK_NULL_HANDLE;
    if (::vkCreateFence(owner, &create, nullptr, &slot.fence_) != VK_SUCCESS) return false;
    slot.device_ = owner;
    return true;
}

uint64_t VulkanSubmitProgress::reserve(uint64_t device)
{
    AV_SCOPE("VulkanSubmitProgress::reserve");
    const uint32_t at = vacancy();
    if (at == slots) return 0u;
    Slot &slot = slots_[at];
    if (!fenced(slot, reinterpret_cast<VkDevice>(device))) return 0u;
    slot.mark_ = ++marked_;
    slot.state_ = State::reserved;
    return slot.mark_;
}

uint32_t VulkanSubmitProgress::reserved_for(uint64_t mark) const
{
    AV_SCOPE("VulkanSubmitProgress::reserved_for");
    for (uint32_t index = 0u; index < slots; index++)
        if (slots_[index].state_ == State::reserved && slots_[index].mark_ == mark) return index;
    return slots;
}

void VulkanSubmitProgress::commit(uint64_t mark, uint64_t queue)
{
    AV_SCOPE("VulkanSubmitProgress::commit");
    const uint32_t at = reserved_for(mark);
    if (at == slots) return;
    Slot &slot = slots_[at];
    slot.queue_ = reinterpret_cast<VkQueue>(queue);
    slot.state_ = State::stuck;
    static_cast<void>(queued(slot));
}

bool VulkanSubmitProgress::queued(Slot &slot)
{
    AV_SCOPE("VulkanSubmitProgress::queued");
    const VkResult status = ::vkQueueSubmit(slot.queue_, 0u, nullptr, slot.fence_);
    if (status == VK_ERROR_DEVICE_LOST) slot.state_ = State::vacant;
    else if (status == VK_SUCCESS) slot.state_ = State::pending;
    return status == VK_SUCCESS || status == VK_ERROR_DEVICE_LOST;
}

bool VulkanSubmitProgress::settled(Slot &slot)
{
    AV_SCOPE("VulkanSubmitProgress::settled");
    if (slot.state_ == State::stuck && !queued(slot)) {
        if (::vkDeviceWaitIdle(slot.device_) != VK_SUCCESS) return false;
        slot.state_ = State::vacant;
        return true;
    }
    if (slot.state_ == State::vacant) return true;
    return slot.state_ == State::pending && waited(slot);
}

void VulkanSubmitProgress::cancel(uint64_t mark)
{
    AV_SCOPE("VulkanSubmitProgress::cancel");
    const uint32_t at = reserved_for(mark);
    if (at != slots) slots_[at].state_ = State::vacant;
}

bool VulkanSubmitProgress::wait_idle(uint64_t device)
{
    AV_SCOPE("VulkanSubmitProgress::wait_idle");
    for (Slot &slot : slots_) {
        if (slot.state_ == State::vacant || number(slot.device_) != device) continue;
        if (!settled(slot)) return false;
    }
    return true;
}

bool VulkanSubmitProgress::wait_marks(uint64_t device, const uint64_t *marks, uint32_t count)
{
    AV_SCOPE("VulkanSubmitProgress::wait_marks");
    for (Slot &slot : slots_) {
        if (slot.state_ == State::vacant || number(slot.device_) != device) continue;
        bool stamped = false;
        for (uint32_t index = 0u; index < count && !stamped; index++) stamped = marks[index] == slot.mark_;
        if (stamped && !settled(slot)) return false;
    }
    return true;
}

uint32_t VulkanSubmitProgress::stuck(uint64_t device) const
{
    AV_SCOPE("VulkanSubmitProgress::stuck");
    uint32_t found = 0u;
    for (const Slot &slot : slots_)
        if (slot.state_ == State::stuck && number(slot.device_) == device) found++;
    return found;
}

bool VulkanSubmitProgress::settled_within(Slot &slot, TeardownBudget &budget)
{
    AV_SCOPE("VulkanSubmitProgress::settled_within");
    if (slot.state_ == State::stuck && !queued(slot)) return false;
    if (slot.state_ == State::vacant) return true;
    return slot.state_ == State::pending && waited(slot, budget.remaining());
}

bool VulkanSubmitProgress::drain(uint64_t device, TeardownBudget &budget)
{
    AV_SCOPE("VulkanSubmitProgress::drain");
    bool drained = true;
    for (Slot &slot : slots_) {
        if (slot.state_ == State::vacant || number(slot.device_) != device) continue;
        if (!settled_within(slot, budget)) drained = false;
    }
    return drained;
}

void VulkanSubmitProgress::forget(VkDevice device)
{
    AV_SCOPE("VulkanSubmitProgress::forget");
    for (Slot &slot : slots_) {
        if (slot.fence_ == VK_NULL_HANDLE || slot.device_ != device) continue;
        const bool idle = slot.state_ != State::pending || waited(slot);
        if (idle) ::vkDestroyFence(device, slot.fence_, nullptr);
        slot = Slot{};
    }
}

bool VulkanSubmitProgress::invariants_hold() const
{
    AV_SCOPE("VulkanSubmitProgress::invariants_hold");
    for (const Slot &slot : slots_)
        if (slot.state_ != State::vacant && (slot.fence_ == VK_NULL_HANDLE || slot.mark_ == 0u || slot.mark_ > marked_))
            return false;
    return true;
}

}
