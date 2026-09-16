#include "icd/vulkan/memory_type_table.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/kmt/memory_segment_group.hpp"

namespace av::icd {

namespace {

constexpr uint32_t local_group = static_cast<uint32_t>(MemorySegmentGroup::local);
constexpr uint32_t non_local_group = static_cast<uint32_t>(MemorySegmentGroup::non_local);

}  // namespace

MemoryTypeTable::MemoryTypeTable(Kmt &kmt, uint32_t adapter)
    : kmt_(kmt), adapter_(adapter), properties_(), budgets_(), usages_()
    {
        AV_SCOPE("MemoryTypeTable::MemoryTypeTable");
    }

void MemoryTypeTable::clear()
{
    AV_SCOPE("MemoryTypeTable::clear");
    properties_ = VkPhysicalDeviceMemoryProperties();
    for (uint32_t group = 0u; group < groups; group++) {
        budgets_[group] = 0u;
        usages_[group] = 0u;
    }
}

bool MemoryTypeTable::acceptable(const VkPhysicalDeviceMemoryProperties &host) const
{
    AV_SCOPE("MemoryTypeTable::acceptable");
    if (host.memoryTypeCount > VK_MAX_MEMORY_TYPES || host.memoryHeapCount > VK_MAX_MEMORY_HEAPS) return false;
    if (host.memoryHeapCount == 0u) return false;
    for (uint32_t type = 0u; type < host.memoryTypeCount; type++)
        if (host.memoryTypes[type].heapIndex >= host.memoryHeapCount) return false;
    return true;
}

uint32_t MemoryTypeTable::group_of(const VkMemoryHeap &heap) const
{
    AV_SCOPE("MemoryTypeTable::group_of");
    return (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0u ? local_group : non_local_group;
}

Status MemoryTypeTable::build(const VkPhysicalDeviceMemoryProperties &host)
{
    AV_SCOPE("MemoryTypeTable::build");
    clear();
    uint64_t     budgets[groups] = {};
    uint64_t     usages[groups] = {};
    const Status local = kmt_.query_video_memory(adapter_, MemorySegmentGroup::local, budgets[local_group],
                                                 usages[local_group]);
    if (local != Status::ok) return local;
    const Status shared = kmt_.query_video_memory(adapter_, MemorySegmentGroup::non_local, budgets[non_local_group],
                                                  usages[non_local_group]);
    if (shared != Status::ok) return shared;
    if (!acceptable(host)) return Status::malformed;

    properties_.memoryTypeCount = host.memoryTypeCount;
    for (uint32_t type = 0u; type < host.memoryTypeCount; type++) properties_.memoryTypes[type] = host.memoryTypes[type];
    properties_.memoryHeapCount = host.memoryHeapCount;
    for (uint32_t heap = 0u; heap < host.memoryHeapCount; heap++) {
        properties_.memoryHeaps[heap].flags = host.memoryHeaps[heap].flags;
        properties_.memoryHeaps[heap].size = budgets[group_of(host.memoryHeaps[heap])];
    }
    for (uint32_t group = 0u; group < groups; group++) {
        budgets_[group] = budgets[group];
        usages_[group] = usages[group];
    }
    return Status::ok;
}

void MemoryTypeTable::properties(VkPhysicalDeviceMemoryProperties &answer) const
{
    AV_SCOPE("MemoryTypeTable::properties");
    answer = properties_;
}

void MemoryTypeTable::budget(VkPhysicalDeviceMemoryBudgetPropertiesEXT &answer) const
{
    AV_SCOPE("MemoryTypeTable::budget");
    for (uint32_t heap = 0u; heap < VK_MAX_MEMORY_HEAPS; heap++) {
        const bool     listed = heap < properties_.memoryHeapCount;
        const uint32_t group = group_of(properties_.memoryHeaps[heap]);
        answer.heapBudget[heap] = listed ? budgets_[group] : 0u;
        answer.heapUsage[heap] = listed ? usages_[group] : 0u;
    }
}

bool MemoryTypeTable::host_visible(uint32_t type) const
{
    AV_SCOPE("MemoryTypeTable::host_visible");
    if (type >= properties_.memoryTypeCount) return false;
    return (properties_.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u;
}

bool MemoryTypeTable::invariants_hold() const
{
    AV_SCOPE("MemoryTypeTable::invariants_hold");
    if (properties_.memoryTypeCount > VK_MAX_MEMORY_TYPES || properties_.memoryHeapCount > VK_MAX_MEMORY_HEAPS)
        return false;
    if (properties_.memoryHeapCount == 0u) return properties_.memoryTypeCount == 0u;
    for (uint32_t type = 0u; type < properties_.memoryTypeCount; type++)
        if (properties_.memoryTypes[type].heapIndex >= properties_.memoryHeapCount) return false;
    for (uint32_t heap = 0u; heap < properties_.memoryHeapCount; heap++)
        if (properties_.memoryHeaps[heap].size != budgets_[group_of(properties_.memoryHeaps[heap])]) return false;
    return true;
}

}  // namespace av::icd
