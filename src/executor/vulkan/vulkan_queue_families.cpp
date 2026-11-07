#include "executor/vulkan/vulkan_queue_families.hpp"

#include "core/trace/function_scope.hpp"

#include <vulkan/vulkan_core.h>

namespace av {

VulkanQueueFamilies::VulkanQueueFamilies(BlockSource &blocks) : pools_(blocks, Pool::paged)
{
    AV_SCOPE("VulkanQueueFamilies::VulkanQueueFamilies");
}

Status VulkanQueueFamilies::record(uint64_t pool, uint32_t family_flags)
{
    AV_SCOPE("VulkanQueueFamilies::record");
    if (pool == 0u) return Status::invalid_parameter;
    const uint32_t implied = (family_flags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) != 0u ? VK_QUEUE_TRANSFER_BIT : 0u;
    uint32_t *const known = pools_.find(pool);
    if (known != nullptr) {
        *known = family_flags | implied;
        return Status::ok;
    }
    uint32_t slot = 0u;
    return pools_.insert(pool, family_flags | implied, slot);
}

void VulkanQueueFamilies::forget(uint64_t pool)
{
    AV_SCOPE("VulkanQueueFamilies::forget");
    static_cast<void>(pools_.erase(pool));
}

void VulkanQueueFamilies::clear()
{
    AV_SCOPE("VulkanQueueFamilies::clear");
    pools_.clear();
}

bool VulkanQueueFamilies::admits(uint64_t pool, uint32_t queues) const
{
    AV_SCOPE("VulkanQueueFamilies::admits");
    const uint32_t *const flags = pools_.find(pool);
    if (flags == nullptr) return false;
    return queues == 0u || (*flags & queues) != 0u;
}

uint32_t VulkanQueueFamilies::count() const
{
    AV_SCOPE("VulkanQueueFamilies::count");
    return pools_.count();
}

bool VulkanQueueFamilies::invariants_hold() const
{
    AV_SCOPE("VulkanQueueFamilies::invariants_hold");
    return pools_.invariants_hold();
}

}
