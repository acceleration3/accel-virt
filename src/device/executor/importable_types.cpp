#include "device/executor/importable_types.hpp"

namespace av {

namespace {

constexpr VkMemoryPropertyFlags host_only = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                            VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

}  // namespace

ImportableTypes::ImportableTypes() : physical_(0u), mask_(0u) {}

void ImportableTypes::learn(uint64_t physical, uint32_t mask)
{
    physical_ = physical;
    mask_ = mask;
}

bool ImportableTypes::known_for(uint64_t physical) const { return physical_ != 0u && physical_ == physical; }

bool ImportableTypes::includes(uint32_t type) const
{
    return type < VK_MAX_MEMORY_TYPES && (mask_ & (uint32_t{1} << type)) != 0u;
}

void ImportableTypes::restrict(VkPhysicalDeviceMemoryProperties &properties) const
{
    for (uint32_t type = 0u; type < properties.memoryTypeCount && type < VK_MAX_MEMORY_TYPES; type++)
        if (!includes(type)) properties.memoryTypes[type].propertyFlags &= ~host_only;
}

bool ImportableTypes::invariants_hold() const { return physical_ != 0u || mask_ == 0u; }

}  // namespace av
