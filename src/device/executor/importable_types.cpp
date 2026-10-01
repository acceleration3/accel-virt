#include "device/executor/importable_types.hpp"

#include "core/trace/function_scope.hpp"
#include "device/abi/av_executor.h"

namespace av {

namespace {

constexpr VkMemoryPropertyFlags host_only = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                            VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

}  // namespace

ImportableTypes::ImportableTypes() : physical_(0u), mask_(0u), kind_(0u)
{
    AV_SCOPE("ImportableTypes::ImportableTypes");
}

void ImportableTypes::learn(uint64_t physical, uint32_t by_dmabuf, uint32_t by_pointer)
{
    AV_SCOPE("ImportableTypes::learn");
    physical_ = physical;
    mask_ = by_dmabuf != 0u ? by_dmabuf : by_pointer;
    kind_ = by_dmabuf != 0u ? AV_BLOB_DMA_BUF : AV_BLOB_HOST_POINTER;
}

uint32_t ImportableTypes::kind() const
{
    AV_SCOPE("ImportableTypes::kind");
    return physical_ != 0u ? kind_ : 0u;
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
    if (physical_ == 0u) return mask_ == 0u;
    return kind_ == AV_BLOB_DMA_BUF || kind_ == AV_BLOB_HOST_POINTER;
}

}  // namespace av
