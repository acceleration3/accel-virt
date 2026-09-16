#include "device/executor/importable_types.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr VkMemoryPropertyFlags host_only = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                            VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

}  // namespace

ImportableTypes::ImportableTypes() : physical_(0u), mask_(0u)
{
    AV_SCOPE("ImportableTypes::ImportableTypes");
}

void ImportableTypes::learn(uint64_t physical, uint32_t mask)
{
    AV_SCOPE("ImportableTypes::learn");
    physical_ = physical;
    mask_ = mask;
}

bool ImportableTypes::known_for(uint64_t physical) const
{
    AV_SCOPE("ImportableTypes::known_for");
    return physical_ != 0u && physical_ == physical;
}

bool ImportableTypes::includes(uint32_t type) const
{
    AV_SCOPE("ImportableTypes::includes");
    return type < VK_MAX_MEMORY_TYPES && (mask_ & (uint32_t{1} << type)) != 0u;
}

void ImportableTypes::restrict(VkPhysicalDeviceMemoryProperties &properties) const
{
    AV_SCOPE("ImportableTypes::restrict");
    for (uint32_t type = 0u; type < properties.memoryTypeCount && type < VK_MAX_MEMORY_TYPES; type++)
        if (!includes(type)) properties.memoryTypes[type].propertyFlags &= ~host_only;
}

bool ImportableTypes::invariants_hold() const
{
    AV_SCOPE("ImportableTypes::invariants_hold");
    return physical_ != 0u || mask_ == 0u;
}

}  // namespace av
