#include "executor/vulkan/guest_memory_types.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

GuestMemoryTypes::GuestMemoryTypes() : properties_(), hosts_{}, backed_(0u), host_types_(0u)
{
    AV_SCOPE("GuestMemoryTypes::GuestMemoryTypes");
}

void GuestMemoryTypes::learn(const VkPhysicalDeviceMemoryProperties &host, uint32_t backed)
{
    AV_SCOPE("GuestMemoryTypes::learn");
    properties_ = VkPhysicalDeviceMemoryProperties();
    backed_ = 0u;
    host_types_ = host.memoryTypeCount < VK_MAX_MEMORY_TYPES ? host.memoryTypeCount : VK_MAX_MEMORY_TYPES;
    properties_.memoryHeapCount = host.memoryHeapCount < VK_MAX_MEMORY_HEAPS ? host.memoryHeapCount
                                                                             : VK_MAX_MEMORY_HEAPS;
    for (uint32_t heap = 0u; heap < properties_.memoryHeapCount; heap++)
        properties_.memoryHeaps[heap] = host.memoryHeaps[heap];
    uint32_t sourced = 0u;
    for (uint32_t type = 0u; type < host_types_; type++)
        if ((host.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u)
            sourced |= backed & (1u << type);
    const uint32_t twins = twin_origins(host, sourced);
    for (uint32_t type = 0u; type < host_types_; type++) {
        const VkMemoryType &kind = host.memoryTypes[type];
        const bool          source = (sourced & (1u << type)) != 0u;
        if ((twins & (1u << type)) != 0u) append(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, kind.heapIndex, type, false);
        append(source ? kind.propertyFlags : kind.propertyFlags & ~host_access, kind.heapIndex, type, source);
    }
    order();
}

uint32_t GuestMemoryTypes::twin_origins(const VkPhysicalDeviceMemoryProperties &host, uint32_t backed) const
{
    AV_SCOPE("GuestMemoryTypes::twin_origins");
    uint32_t origins = 0u;
    uint32_t room = VK_MAX_MEMORY_TYPES - host_types_;
    for (uint32_t heap = 0u; heap < properties_.memoryHeapCount && room != 0u; heap++) {
        bool     served = false;
        uint32_t origin = no_type;
        for (uint32_t type = 0u; type < host_types_; type++) {
            const VkMemoryType &kind = host.memoryTypes[type];
            if (kind.heapIndex != heap) continue;
            const bool sourced = (backed & (1u << type)) != 0u;
            served = served || (sourced ? kind.propertyFlags : kind.propertyFlags & ~host_access) ==
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            if (origin == no_type && sourced &&
                (kind.propertyFlags & ~host_access) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                origin = type;
        }
        if (served || origin == no_type) continue;
        origins |= 1u << origin;
        room--;
    }
    return origins;
}

void GuestMemoryTypes::append(VkMemoryPropertyFlags flags, uint32_t heap, uint32_t host, bool backed)
{
    AV_SCOPE("GuestMemoryTypes::append");
    const uint32_t type = properties_.memoryTypeCount++;
    properties_.memoryTypes[type].propertyFlags = flags;
    properties_.memoryTypes[type].heapIndex = heap;
    hosts_[type] = host;
    if (backed) backed_ |= 1u << type;
}

void GuestMemoryTypes::order()
{
    AV_SCOPE("GuestMemoryTypes::order");
    for (uint32_t position = 0u; position < properties_.memoryTypeCount; position++)
        move_to(ready_from(position), position);
}

uint32_t GuestMemoryTypes::ready_from(uint32_t first) const
{
    AV_SCOPE("GuestMemoryTypes::ready_from");
    uint32_t candidate = first;
    while (preceded(candidate, first)) candidate++;
    return candidate;
}

bool GuestMemoryTypes::preceded(uint32_t type, uint32_t first) const
{
    AV_SCOPE("GuestMemoryTypes::preceded");
    for (uint32_t other = first; other < properties_.memoryTypeCount; other++)
        if (strict_subset(properties_.memoryTypes[other].propertyFlags, properties_.memoryTypes[type].propertyFlags))
            return true;
    return false;
}

void GuestMemoryTypes::move_to(uint32_t from, uint32_t to)
{
    AV_SCOPE("GuestMemoryTypes::move_to");
    const VkMemoryType moved = properties_.memoryTypes[from];
    const uint32_t     host = hosts_[from];
    const bool         backed = (backed_ & (1u << from)) != 0u;
    for (uint32_t type = from; type > to; type--) {
        properties_.memoryTypes[type] = properties_.memoryTypes[type - 1u];
        hosts_[type] = hosts_[type - 1u];
        backed_ = (backed_ & ~(1u << type)) | (((backed_ >> (type - 1u)) & 1u) << type);
    }
    properties_.memoryTypes[to] = moved;
    hosts_[to] = host;
    backed_ = (backed_ & ~(1u << to)) | (backed ? 1u << to : 0u);
}

bool GuestMemoryTypes::strict_subset(VkMemoryPropertyFlags inner, VkMemoryPropertyFlags outer) const
{
    AV_SCOPE("GuestMemoryTypes::strict_subset");
    return inner != outer && (inner & outer) == inner;
}

void GuestMemoryTypes::properties(VkPhysicalDeviceMemoryProperties &answer) const
{
    AV_SCOPE("GuestMemoryTypes::properties");
    answer = properties_;
}

uint32_t GuestMemoryTypes::host_type(uint32_t guest) const
{
    AV_SCOPE("GuestMemoryTypes::host_type");
    return guest < properties_.memoryTypeCount ? hosts_[guest] : no_type;
}

uint32_t GuestMemoryTypes::heap_of(uint32_t guest) const
{
    AV_SCOPE("GuestMemoryTypes::heap_of");
    return guest < properties_.memoryTypeCount ? properties_.memoryTypes[guest].heapIndex : no_type;
}

uint32_t GuestMemoryTypes::source_type(uint32_t guest) const
{
    AV_SCOPE("GuestMemoryTypes::source_type");
    return guest < properties_.memoryTypeCount && (backed_ & (1u << guest)) != 0u ? hosts_[guest] : no_type;
}

uint32_t GuestMemoryTypes::guest_bits(uint32_t host_bits) const
{
    AV_SCOPE("GuestMemoryTypes::guest_bits");
    uint32_t bits = 0u;
    for (uint32_t type = 0u; type < properties_.memoryTypeCount; type++)
        if ((host_bits & (1u << hosts_[type])) != 0u) bits |= 1u << type;
    return bits;
}

uint32_t GuestMemoryTypes::unmapped(uint32_t guest_bits) const
{
    AV_SCOPE("GuestMemoryTypes::unmapped");
    const uint32_t native = guest_bits & ~backed_;
    return native != 0u ? native : guest_bits;
}

bool GuestMemoryTypes::invariants_hold() const
{
    AV_SCOPE("GuestMemoryTypes::invariants_hold");
    const uint32_t count = properties_.memoryTypeCount;
    if (count > VK_MAX_MEMORY_TYPES || properties_.memoryHeapCount > VK_MAX_MEMORY_HEAPS || host_types_ > count)
        return false;
    if (count < VK_MAX_MEMORY_TYPES && (backed_ >> count) != 0u) return false;
    uint32_t shown = 0u;
    for (uint32_t type = 0u; type < count; type++) {
        const VkMemoryPropertyFlags flags = properties_.memoryTypes[type].propertyFlags;
        const bool                  backed = (backed_ & (1u << type)) != 0u;
        if (hosts_[type] >= host_types_) return false;
        if (backed != ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u)) return false;
        for (uint32_t earlier = 0u; earlier < type; earlier++)
            if (strict_subset(flags, properties_.memoryTypes[earlier].propertyFlags)) return false;
        shown |= 1u << hosts_[type];
    }
    return host_types_ == VK_MAX_MEMORY_TYPES ? shown == 0xFFFFFFFFu : shown == (1u << host_types_) - 1u;
}

}
