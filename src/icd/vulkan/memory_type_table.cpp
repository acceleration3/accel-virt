#include "icd/vulkan/memory_type_table.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/vulkan/placed_memory_types.hpp"

namespace av::icd {

MemoryTypeTable::MemoryTypeTable(Kmt &kmt, uint32_t adapter) : memory_(kmt, adapter), properties_(), shared_budget_(0u), placed_(0u)
{
    AV_SCOPE("MemoryTypeTable::MemoryTypeTable");
}

void MemoryTypeTable::clear()
{
    AV_SCOPE("MemoryTypeTable::clear");
    properties_ = VkPhysicalDeviceMemoryProperties();
    shared_budget_ = 0u;
    placed_ = 0u;
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

MemorySegmentGroup MemoryTypeTable::group_of(const VkMemoryHeap &heap) const
{
    AV_SCOPE("MemoryTypeTable::group_of");
    return (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0u ? MemorySegmentGroup::local
                                                                : MemorySegmentGroup::non_local;
}

Status MemoryTypeTable::build(const VkPhysicalDeviceMemoryProperties &host)
{
    AV_SCOPE("MemoryTypeTable::build");
    clear();
    const Status read = memory_.read();
    if (read != Status::ok) return read;
    if (!acceptable(host)) return Status::malformed;

    properties_.memoryTypeCount = host.memoryTypeCount;
    for (uint32_t type = 0u; type < host.memoryTypeCount; type++) properties_.memoryTypes[type] = host.memoryTypes[type];
    properties_.memoryHeapCount = host.memoryHeapCount;
    for (uint32_t heap = 0u; heap < host.memoryHeapCount; heap++) {
        properties_.memoryHeaps[heap].flags = host.memoryHeaps[heap].flags;
        properties_.memoryHeaps[heap].size = host.memoryHeaps[heap].size;
    }
    shared_budget_ = memory_.budget(MemorySegmentGroup::non_local);
    placed_ = PlacedMemoryTypes::mask(properties_);
    return Status::ok;
}

void MemoryTypeTable::properties(VkPhysicalDeviceMemoryProperties &answer) const
{
    AV_SCOPE("MemoryTypeTable::properties");
    answer = properties_;
}

void MemoryTypeTable::budget(VkPhysicalDeviceMemoryBudgetPropertiesEXT &answer,
                             const VkPhysicalDeviceMemoryBudgetPropertiesEXT &hosted) const
{
    AV_SCOPE("MemoryTypeTable::budget");
    for (uint32_t heap = 0u; heap < VK_MAX_MEMORY_HEAPS; heap++) {
        const bool listed = heap < properties_.memoryHeapCount;
        const uint64_t adapter = listed ? memory_.usage(group_of(properties_.memoryHeaps[heap])) : 0u;
        const uint64_t usage = listed && hosted.heapUsage[heap] > adapter ? hosted.heapUsage[heap] : adapter;
        uint64_t size = listed ? properties_.memoryHeaps[heap].size : 0u;
        if (listed && group_of(properties_.memoryHeaps[heap]) == MemorySegmentGroup::non_local && shared_budget_ != 0u &&
            shared_budget_ < size)
            size = shared_budget_;
        answer.heapUsage[heap] = usage;
        answer.heapBudget[heap] = listed && size < usage ? usage : size;
    }
}

bool MemoryTypeTable::host_visible(uint32_t type) const
{
    AV_SCOPE("MemoryTypeTable::host_visible");
    if (type >= properties_.memoryTypeCount) return false;
    return (properties_.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u;
}

bool MemoryTypeTable::placed(uint32_t type) const
{
    AV_SCOPE("MemoryTypeTable::placed");
    return type < properties_.memoryTypeCount && (placed_ & (1u << type)) != 0u;
}

uint32_t MemoryTypeTable::lendable_types() const
{
    AV_SCOPE("MemoryTypeTable::lendable_types");
    constexpr VkMemoryPropertyFlags wanted = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                             VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    uint32_t types = 0u;
    for (uint32_t type = 0u; type < properties_.memoryTypeCount; type++)
        if ((properties_.memoryTypes[type].propertyFlags & wanted) == wanted) types |= 1u << type;
    return types;
}

bool MemoryTypeTable::invariants_hold() const
{
    AV_SCOPE("MemoryTypeTable::invariants_hold");
    if (!memory_.invariants_hold()) return false;
    if (properties_.memoryTypeCount > VK_MAX_MEMORY_TYPES || properties_.memoryHeapCount > VK_MAX_MEMORY_HEAPS)
        return false;
    if (properties_.memoryHeapCount == 0u) return properties_.memoryTypeCount == 0u;
    for (uint32_t type = 0u; type < properties_.memoryTypeCount; type++)
        if (properties_.memoryTypes[type].heapIndex >= properties_.memoryHeapCount) return false;
    return true;
}

}
