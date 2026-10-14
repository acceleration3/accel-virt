#include "harness/executor/guest_memory_types_probe.hpp"

namespace av {

GuestMemoryTypesProbe::GuestMemoryTypesProbe(GuestMemoryTypes &target) : types(target) {}

void GuestMemoryTypesProbe::set_type_count(uint32_t count) { types.properties_.memoryTypeCount = count; }
void GuestMemoryTypesProbe::set_heap_count(uint32_t count) { types.properties_.memoryHeapCount = count; }
void GuestMemoryTypesProbe::set_host_types(uint32_t count) { types.host_types_ = count; }
void GuestMemoryTypesProbe::set_backed(uint32_t mask) { types.backed_ = mask; }
void GuestMemoryTypesProbe::set_host(uint32_t guest, uint32_t host) { types.hosts_[guest] = host; }
void GuestMemoryTypesProbe::set_flags(uint32_t guest, VkMemoryPropertyFlags flags)
{
    types.properties_.memoryTypes[guest].propertyFlags = flags;
}

}  // namespace av
