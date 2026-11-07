#include "executor/vulkan/vulkan_queue_timelines.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t most_waited = 32u;
constexpr uint64_t wait_ns = 5000000000u;

uint64_t number(const void *handle)
{
    AV_SCOPE("number");
    return reinterpret_cast<uint64_t>(handle);
}

}

VulkanQueueTimelines::VulkanQueueTimelines(BlockSource &blocks)
    : devices_(blocks, Pool::non_paged), lines_(blocks, Pool::non_paged), open_(blocks, Pool::non_paged),
      pending_(blocks, Pool::non_paged), lost_(false)
{
    AV_SCOPE("VulkanQueueTimelines::VulkanQueueTimelines");
}

Status VulkanQueueTimelines::enable(VkDevice device)
{
    AV_SCOPE("VulkanQueueTimelines::enable");
    if (enabled(number(device))) return Status::ok;
    return devices_.append(device);
}

bool VulkanQueueTimelines::enabled(uint64_t device) const
{
    AV_SCOPE("VulkanQueueTimelines::enabled");
    for (const VkDevice listed : devices_)
        if (number(listed) == device) return true;
    return false;
}

VulkanQueueTimelines::Line *VulkanQueueTimelines::line_of(uint64_t queue)
{
    AV_SCOPE("VulkanQueueTimelines::line_of");
    for (Line &line : lines_)
        if (line.semaphore_ != VK_NULL_HANDLE && number(line.queue_) == queue) return &line;
    return nullptr;
}

bool VulkanQueueTimelines::acquire(uint64_t device, uint64_t queue, VkSemaphore &semaphore, uint64_t &value)
{
    AV_SCOPE("VulkanQueueTimelines::acquire");
    if (!enabled(device)) return false;
    Line *line = line_of(queue);
    if (line == nullptr) {
        VkSemaphoreTypeCreateInfo type = {};
        type.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo create = {};
        create.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        create.pNext = &type;
        VkSemaphore made = VK_NULL_HANDLE;
        if (::vkCreateSemaphore(reinterpret_cast<VkDevice>(device), &create, nullptr, &made) != VK_SUCCESS) return false;
        const Line fresh{reinterpret_cast<VkDevice>(device), reinterpret_cast<VkQueue>(queue), made, 0u, 0u};
        for (Line &spare : lines_)
            if (spare.semaphore_ == VK_NULL_HANDLE) {
                spare = fresh;
                line = &spare;
                break;
            }
        if (line == nullptr) {
            if (lines_.append(fresh) != Status::ok) {
                ::vkDestroySemaphore(reinterpret_cast<VkDevice>(device), made, nullptr);
                return false;
            }
            line = &lines_.at(lines_.size() - 1u);
        }
    }
    semaphore = line->semaphore_;
    value = line->issued_ + 1u;
    return true;
}

Status VulkanQueueTimelines::open(uint64_t mark, uint64_t device)
{
    AV_SCOPE("VulkanQueueTimelines::open");
    return open_.append(Open{mark, device});
}

void VulkanQueueTimelines::close(uint64_t mark)
{
    AV_SCOPE("VulkanQueueTimelines::close");
    for (uint32_t at = open_.size(); at != 0u; at--)
        if (open_.at(at - 1u).mark_ == mark) return open_.remove_at(at - 1u);
}

bool VulkanQueueTimelines::commit(uint64_t mark, uint64_t queue, uint64_t value)
{
    AV_SCOPE("VulkanQueueTimelines::commit");
    Line *const line = line_of(queue);
    if (line == nullptr || value != line->issued_ + 1u) return false;
    uint32_t index = 0u;
    for (const Line &listed : lines_) {
        if (&listed == line) break;
        index++;
    }
    if (pending_.append(Pending{index, mark, value}) != Status::ok) return false;
    line->issued_ = value;
    close(mark);
    return true;
}

void VulkanQueueTimelines::note_status(VkResult status)
{
    AV_SCOPE("VulkanQueueTimelines::note_status");
    if (status == VK_ERROR_DEVICE_LOST) lost_ = true;
}

void VulkanQueueTimelines::poll()
{
    AV_SCOPE("VulkanQueueTimelines::poll");
    if (pending_.size() == 0u) return;
    for (Line &line : lines_) {
        if (line.semaphore_ == VK_NULL_HANDLE || line.issued_ == line.reached_) continue;
        uint64_t counter = line.reached_;
        const VkResult status = ::vkGetSemaphoreCounterValue(line.device_, line.semaphore_, &counter);
        note_status(status);
        if (status == VK_SUCCESS && counter > line.reached_) line.reached_ = counter;
    }
    uint32_t kept = 0u;
    for (uint32_t at = 0u; at < pending_.size(); at++) {
        const Pending entry = pending_.at(at);
        if (entry.value_ <= lines_.at(entry.line_).reached_) continue;
        pending_.at(kept++) = entry;
    }
    pending_.truncate(kept);
}

uint64_t VulkanQueueTimelines::floor(uint64_t device, uint64_t ceiling) const
{
    AV_SCOPE("VulkanQueueTimelines::floor");
    uint64_t reached = ceiling;
    for (const Open &entry : open_)
        if (entry.device_ == device && entry.mark_ - 1u < reached) reached = entry.mark_ - 1u;
    for (const Pending &entry : pending_)
        if (number(lines_.at(entry.line_).device_) == device && entry.mark_ - 1u < reached) reached = entry.mark_ - 1u;
    return reached;
}

bool VulkanQueueTimelines::open_for(uint64_t device) const
{
    AV_SCOPE("VulkanQueueTimelines::open_for");
    for (const Open &entry : open_)
        if (entry.device_ == device) return true;
    return false;
}

bool VulkanQueueTimelines::waited(VkDevice device, const VkSemaphore *semaphores, const uint64_t *values,
                                  uint32_t count, uint64_t nanoseconds)
{
    AV_SCOPE("VulkanQueueTimelines::waited");
    if (count == 0u) return true;
    VkSemaphoreWaitInfo wait = {};
    wait.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    wait.semaphoreCount = count;
    wait.pSemaphores = semaphores;
    wait.pValues = values;
    const VkResult status = ::vkWaitSemaphores(device, &wait, nanoseconds);
    note_status(status);
    return status == VK_SUCCESS;
}

bool VulkanQueueTimelines::settle_device(uint64_t device, uint64_t nanoseconds)
{
    AV_SCOPE("VulkanQueueTimelines::settle_device");
    if (open_for(device)) return false;
    VkSemaphore semaphores[most_waited];
    uint64_t    values[most_waited];
    uint32_t    count = 0u;
    bool        settled = true;
    for (const Line &line : lines_) {
        if (line.semaphore_ == VK_NULL_HANDLE || number(line.device_) != device || line.issued_ == line.reached_) continue;
        semaphores[count] = line.semaphore_;
        values[count++] = line.issued_;
        if (count < most_waited) continue;
        settled = waited(reinterpret_cast<VkDevice>(device), semaphores, values, count, nanoseconds) && settled;
        count = 0u;
    }
    settled = waited(reinterpret_cast<VkDevice>(device), semaphores, values, count, nanoseconds) && settled;
    poll();
    return settled;
}

bool VulkanQueueTimelines::settle_marks(uint64_t device, const uint64_t *marks, uint32_t count, uint64_t nanoseconds)
{
    AV_SCOPE("VulkanQueueTimelines::settle_marks");
    for (const Open &entry : open_)
        for (uint32_t at = 0u; at < count; at++)
            if (entry.device_ == device && entry.mark_ == marks[at]) return false;
    VkSemaphore semaphores[most_waited];
    uint64_t    values[most_waited];
    uint32_t    waiting = 0u;
    bool        settled = true;
    for (uint32_t index = 0u; index < lines_.size(); index++) {
        const Line &line = lines_.at(index);
        if (line.semaphore_ == VK_NULL_HANDLE || number(line.device_) != device) continue;
        uint64_t needed = 0u;
        for (const Pending &entry : pending_) {
            if (entry.line_ != index || entry.value_ <= needed) continue;
            for (uint32_t at = 0u; at < count; at++)
                if (marks[at] == entry.mark_) needed = entry.value_;
        }
        if (needed == 0u) continue;
        semaphores[waiting] = line.semaphore_;
        values[waiting++] = needed;
        if (waiting < most_waited) continue;
        settled = waited(reinterpret_cast<VkDevice>(device), semaphores, values, waiting, nanoseconds) && settled;
        waiting = 0u;
    }
    settled = waited(reinterpret_cast<VkDevice>(device), semaphores, values, waiting, nanoseconds) && settled;
    poll();
    return settled;
}

void VulkanQueueTimelines::forget(VkDevice device)
{
    AV_SCOPE("VulkanQueueTimelines::forget");
    for (uint32_t at = devices_.size(); at != 0u; at--)
        if (devices_.at(at - 1u) == device) devices_.remove_at(at - 1u);
    uint32_t kept = 0u;
    for (uint32_t at = 0u; at < open_.size(); at++) {
        const Open entry = open_.at(at);
        if (entry.device_ != number(device)) open_.at(kept++) = entry;
    }
    open_.truncate(kept);
    kept = 0u;
    for (uint32_t at = 0u; at < pending_.size(); at++) {
        const Pending entry = pending_.at(at);
        if (lines_.at(entry.line_).device_ != device) pending_.at(kept++) = entry;
    }
    pending_.truncate(kept);
    for (Line &line : lines_) {
        if (line.semaphore_ == VK_NULL_HANDLE || line.device_ != device) continue;
        bool idle = line.issued_ == line.reached_;
        if (!idle && !lost_) idle = waited(device, &line.semaphore_, &line.issued_, 1u, wait_ns);
        if (idle) ::vkDestroySemaphore(device, line.semaphore_, nullptr);
        line = Line{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0u, 0u};
    }
}

bool VulkanQueueTimelines::owns(uint64_t device) const
{
    AV_SCOPE("VulkanQueueTimelines::owns");
    for (const Line &line : lines_)
        if (line.semaphore_ != VK_NULL_HANDLE && number(line.device_) == device) return true;
    return open_for(device);
}

bool VulkanQueueTimelines::lost() const
{
    AV_SCOPE("VulkanQueueTimelines::lost");
    return lost_;
}

uint32_t VulkanQueueTimelines::lines() const
{
    AV_SCOPE("VulkanQueueTimelines::lines");
    uint32_t counted = 0u;
    for (const Line &line : lines_)
        if (line.semaphore_ != VK_NULL_HANDLE) counted++;
    return counted;
}

uint32_t VulkanQueueTimelines::outstanding() const
{
    AV_SCOPE("VulkanQueueTimelines::outstanding");
    return pending_.size();
}

bool VulkanQueueTimelines::invariants_hold() const
{
    AV_SCOPE("VulkanQueueTimelines::invariants_hold");
    for (const Pending &entry : pending_)
        if (entry.line_ >= lines_.size() || entry.mark_ == 0u || entry.value_ > lines_.at(entry.line_).issued_ ||
            entry.value_ <= lines_.at(entry.line_).reached_)
            return false;
    for (const Line &line : lines_)
        if (line.reached_ > line.issued_) return false;
    return true;
}

}
