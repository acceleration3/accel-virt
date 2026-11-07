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

VulkanSubmitProgress::VulkanSubmitProgress(BlockSource &blocks)
    : slots_{}, marked_(0u), cached_device_(0u), cached_reached_(0u), cached_(false), stale_(true), timelines_(blocks), lost_(false)
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
    cached_ = false;
}

void VulkanSubmitProgress::poll_all()
{
    AV_SCOPE("VulkanSubmitProgress::poll_all");
    timelines_.poll();
    for (Slot &slot : slots_) {
        if (slot.state_ == State::stuck) static_cast<void>(queued(slot));
        if (slot.state_ != State::pending) continue;
        const VkResult status = ::vkGetFenceStatus(slot.device_, slot.fence_);
        if (status == VK_SUCCESS) static_cast<void>(::vkResetFences(slot.device_, 1u, &slot.fence_));
        if (status == VK_SUCCESS) slot.state_ = State::vacant;
        if (status != VK_ERROR_DEVICE_LOST) continue;
        slot.state_ = State::lost;
        lost_ = true;
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
    if (cached_ && cached_device_ == device) return cached_reached_;
    uint64_t reached = marked_;
    for (const Slot &slot : slots_)
        if (slot.state_ != State::vacant && number(slot.device_) == device && slot.mark_ - 1u < reached)
            reached = slot.mark_ - 1u;
    reached = timelines_.floor(device, reached);
    cached_ = true;
    cached_device_ = device;
    cached_reached_ = reached;
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
    if (status == VK_ERROR_DEVICE_LOST) {
        slot.state_ = State::lost;
        lost_ = true;
    }
    if (status != VK_SUCCESS) return false;
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
    if (timelines_.enabled(device)) {
        if (timelines_.open(marked_ + 1u, device) != Status::ok) return 0u;
        return ++marked_;
    }
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
    if (status == VK_SUCCESS) slot.state_ = State::pending;
    if (status != VK_ERROR_DEVICE_LOST) return status == VK_SUCCESS;
    slot.state_ = State::lost;
    lost_ = true;
    return false;
}

bool VulkanSubmitProgress::settled(Slot &slot)
{
    AV_SCOPE("VulkanSubmitProgress::settled");
    if (slot.state_ == State::stuck && !queued(slot)) {
        if (slot.state_ == State::lost || ::vkDeviceWaitIdle(slot.device_) != VK_SUCCESS) return false;
        slot.state_ = State::vacant;
        return true;
    }
    if (slot.state_ == State::vacant) return true;
    return slot.state_ == State::pending && waited(slot);
}

void VulkanSubmitProgress::cancel(uint64_t mark)
{
    AV_SCOPE("VulkanSubmitProgress::cancel");
    cached_ = false;
    timelines_.close(mark);
    const uint32_t at = reserved_for(mark);
    if (at != slots) slots_[at].state_ = State::vacant;
}

bool VulkanSubmitProgress::wait_idle(uint64_t device)
{
    AV_SCOPE("VulkanSubmitProgress::wait_idle");
    cached_ = false;
    if (timelines_.owns(device) && !timelines_.settle_device(device, wait_ns)) return false;
    for (Slot &slot : slots_) {
        if (slot.state_ == State::vacant || number(slot.device_) != device) continue;
        if (!settled(slot)) return false;
    }
    return true;
}

bool VulkanSubmitProgress::wait_marks(uint64_t device, const uint64_t *marks, uint32_t count)
{
    AV_SCOPE("VulkanSubmitProgress::wait_marks");
    cached_ = false;
    if (timelines_.owns(device) && !timelines_.settle_marks(device, marks, count, wait_ns)) return false;
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
    cached_ = false;
    bool drained = !timelines_.owns(device) || timelines_.settle_device(device, budget.remaining());
    for (Slot &slot : slots_) {
        if (slot.state_ == State::vacant || number(slot.device_) != device) continue;
        if (!settled_within(slot, budget)) drained = false;
    }
    return drained;
}

void VulkanSubmitProgress::forget(VkDevice device)
{
    AV_SCOPE("VulkanSubmitProgress::forget");
    cached_ = false;
    timelines_.forget(device);
    for (Slot &slot : slots_) {
        if (slot.fence_ == VK_NULL_HANDLE || slot.device_ != device) continue;
        const bool idle = slot.state_ == State::lost ? false : slot.state_ != State::pending || waited(slot);
        if (idle) ::vkDestroyFence(device, slot.fence_, nullptr);
        slot = Slot{};
    }
}

bool VulkanSubmitProgress::lost() const
{
    AV_SCOPE("VulkanSubmitProgress::lost");
    return lost_ || timelines_.lost();
}

bool VulkanSubmitProgress::invariants_hold() const
{
    AV_SCOPE("VulkanSubmitProgress::invariants_hold");
    if (!timelines_.invariants_hold()) return false;
    for (const Slot &slot : slots_)
        if (slot.state_ != State::vacant && (slot.fence_ == VK_NULL_HANDLE || slot.mark_ == 0u || slot.mark_ > marked_))
            return false;
    return true;
}

bool VulkanSubmitProgress::timeline(uint64_t device, uint64_t queue, VkSemaphore &semaphore, uint64_t &value)
{
    AV_SCOPE("VulkanSubmitProgress::timeline");
    return timelines_.acquire(device, queue, semaphore, value);
}

bool VulkanSubmitProgress::timelined(uint64_t device) const
{
    AV_SCOPE("VulkanSubmitProgress::timelined");
    return timelines_.enabled(device);
}

void VulkanSubmitProgress::commit_timeline(uint64_t mark, uint64_t queue, uint64_t value)
{
    AV_SCOPE("VulkanSubmitProgress::commit_timeline");
    cached_ = false;
    if (!timelines_.commit(mark, queue, value)) lost_ = true;
}

bool VulkanSubmitProgress::enable_timelines(VkDevice device)
{
    AV_SCOPE("VulkanSubmitProgress::enable_timelines");
    return timelines_.enable(device) == Status::ok;
}

}
