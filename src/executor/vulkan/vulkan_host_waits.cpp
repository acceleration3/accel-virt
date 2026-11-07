#include "executor/vulkan/vulkan_host_waits.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

VulkanHostWaits::VulkanHostWaits(BlockSource &blocks) : waits_(blocks, Pool::paged)
{
    AV_SCOPE("VulkanHostWaits::VulkanHostWaits");
}

Status VulkanHostWaits::record(uint64_t buffer, uint64_t event)
{
    AV_SCOPE("VulkanHostWaits::record");
    if (buffer == 0u || event == 0u) return Status::invalid_parameter;
    if (waits(buffer, event)) return Status::ok;
    return waits_.append(Wait{buffer, event});
}

Status VulkanHostWaits::inherit(uint64_t primary, uint64_t secondary)
{
    AV_SCOPE("VulkanHostWaits::inherit");
    Status inherited = Status::ok;
    for (uint32_t index = 0u; index < waits_.size() && inherited == Status::ok; index++) {
        const Wait wait = waits_.at(index);
        if (wait.buffer_ == secondary) inherited = record(primary, wait.event_);
    }
    return inherited;
}

void VulkanHostWaits::forget_buffer(uint64_t buffer)
{
    AV_SCOPE("VulkanHostWaits::forget_buffer");
    for (uint32_t index = waits_.size(); index != 0u; index--)
        if (waits_.at(index - 1u).buffer_ == buffer) waits_.remove_swap(index - 1u);
}

void VulkanHostWaits::forget_event(uint64_t event)
{
    AV_SCOPE("VulkanHostWaits::forget_event");
    for (uint32_t index = waits_.size(); index != 0u; index--)
        if (waits_.at(index - 1u).event_ == event) waits_.remove_swap(index - 1u);
}

void VulkanHostWaits::clear()
{
    AV_SCOPE("VulkanHostWaits::clear");
    waits_.clear();
}

bool VulkanHostWaits::waits(uint64_t buffer, uint64_t event) const
{
    AV_SCOPE("VulkanHostWaits::waits");
    for (const Wait &wait : waits_)
        if (wait.buffer_ == buffer && wait.event_ == event) return true;
    return false;
}

bool VulkanHostWaits::any() const
{
    AV_SCOPE("VulkanHostWaits::any");
    return waits_.size() != 0u;
}

uint32_t VulkanHostWaits::count() const
{
    AV_SCOPE("VulkanHostWaits::count");
    return waits_.size();
}

bool VulkanHostWaits::invariants_hold() const
{
    AV_SCOPE("VulkanHostWaits::invariants_hold");
    for (uint32_t index = 0u; index < waits_.size(); index++)
        if (waits_.at(index).buffer_ == 0u || waits_.at(index).event_ == 0u) return false;
    return waits_.invariants_hold();
}

}
