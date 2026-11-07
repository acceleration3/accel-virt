#include "executor/vulkan/vulkan_submit_set.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

VulkanSubmitSet::VulkanSubmitSet(BlockSource &blocks)
    : events_(blocks, Pool::paged), waits_(blocks, Pool::paged), signals_(blocks, Pool::paged)
{
    AV_SCOPE("VulkanSubmitSet::VulkanSubmitSet");
}

Status VulkanSubmitSet::add_unique(GrowableArray<uint64_t> &into, uint64_t value)
{
    AV_SCOPE("VulkanSubmitSet::add_unique");
    if (value == 0u) return Status::ok;
    for (const uint64_t held : into)
        if (held == value) return Status::ok;
    return into.append(value);
}

Status VulkanSubmitSet::add_event(uint64_t event)
{
    AV_SCOPE("VulkanSubmitSet::add_event");
    return add_unique(events_, event);
}

Status VulkanSubmitSet::add_wait(uint64_t semaphore)
{
    AV_SCOPE("VulkanSubmitSet::add_wait");
    return add_unique(waits_, semaphore);
}

Status VulkanSubmitSet::add_signal(uint64_t semaphore)
{
    AV_SCOPE("VulkanSubmitSet::add_signal");
    return add_unique(signals_, semaphore);
}

const uint64_t *VulkanSubmitSet::events() const
{
    AV_SCOPE("VulkanSubmitSet::events");
    return events_.begin();
}

uint32_t VulkanSubmitSet::event_count() const
{
    AV_SCOPE("VulkanSubmitSet::event_count");
    return events_.size();
}

const uint64_t *VulkanSubmitSet::waits() const
{
    AV_SCOPE("VulkanSubmitSet::waits");
    return waits_.begin();
}

uint32_t VulkanSubmitSet::wait_count() const
{
    AV_SCOPE("VulkanSubmitSet::wait_count");
    return waits_.size();
}

const uint64_t *VulkanSubmitSet::signals() const
{
    AV_SCOPE("VulkanSubmitSet::signals");
    return signals_.begin();
}

uint32_t VulkanSubmitSet::signal_count() const
{
    AV_SCOPE("VulkanSubmitSet::signal_count");
    return signals_.size();
}

}
