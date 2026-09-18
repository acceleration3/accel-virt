#include "proto/vulkan/descriptor_template_layout.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/foreign_enum.hpp"

namespace av {

namespace {

constexpr uint64_t highest = ~static_cast<uint64_t>(0);

}  // namespace

DescriptorTemplateLayout::DescriptorTemplateLayout() : bytes_(0u), entries_(0u)
{
    AV_SCOPE("DescriptorTemplateLayout::DescriptorTemplateLayout");
}

Status DescriptorTemplateLayout::take(const VkDescriptorUpdateTemplateEntry *entries, uint32_t count)
{
    AV_SCOPE("DescriptorTemplateLayout::take");
    if (count > max_entries) return Status::too_large;
    if (entries == nullptr && count != 0u) return Status::invalid_parameter;
    uint64_t longest = 0u;
    for (uint32_t index = 0u; index < count; index++) {
        uint64_t     end = 0u;
        const Status measured = end_of(entries[index], end);
        if (measured != Status::ok) return measured;
        longest = end > longest ? end : longest;
    }
    bytes_ = longest;
    entries_ = count;
    return Status::ok;
}

uint64_t DescriptorTemplateLayout::bytes() const
{
    AV_SCOPE("DescriptorTemplateLayout::bytes");
    return bytes_;
}

uint32_t DescriptorTemplateLayout::entries() const
{
    AV_SCOPE("DescriptorTemplateLayout::entries");
    return entries_;
}

/* The sizes the specification gives a descriptor written through a template
 * (Vulkan 1.3, VkDescriptorUpdateTemplateEntry). A mutable descriptor is not
 * among them: a template may not write one. */
uint64_t DescriptorTemplateLayout::descriptor_bytes(int32_t type) const
{
    AV_SCOPE("DescriptorTemplateLayout::descriptor_bytes");
    switch (type) {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return sizeof(VkDescriptorImageInfo);
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        return sizeof(VkBufferView);
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        return sizeof(VkDescriptorBufferInfo);
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        return sizeof(VkAccelerationStructureKHR);
    /* An inline uniform block's descriptorCount is its byte count, so one
     * "descriptor" of it is one byte and the stride is not read at all. */
    case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
        return 1u;
    default:
        return 0u;
    }
}

Status DescriptorTemplateLayout::end_of(const VkDescriptorUpdateTemplateEntry &entry, uint64_t &end) const
{
    AV_SCOPE("DescriptorTemplateLayout::end_of");
    /* The type is an application's field: read as the integer it is, never
     * loaded as the enumeration (proto/wire/foreign_enum.hpp). */
    const int32_t  type = foreign_enum(entry.descriptorType);
    const uint64_t one = descriptor_bytes(type);
    if (one == 0u) return Status::not_supported;
    end = 0u;
    if (entry.descriptorCount == 0u) return Status::ok;
    const uint64_t offset = static_cast<uint64_t>(entry.offset);
    const uint64_t count = entry.descriptorCount;
    /* An inline uniform block is `descriptorCount` bytes from the offset; every
     * other type is one descriptor every `stride` bytes, and the last one ends
     * a descriptor past its own start. */
    const bool     inline_block = type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK;
    const uint64_t stride = inline_block ? 0u : static_cast<uint64_t>(entry.stride);
    const uint64_t steps = inline_block ? 0u : count - 1u;
    if (steps != 0u && stride > highest / steps) return Status::too_large;
    const uint64_t spread = steps * stride;
    const uint64_t last = inline_block ? count : one;
    if (spread > highest - last) return Status::too_large;
    const uint64_t within = spread + last;
    if (offset > highest - within) return Status::too_large;
    end = offset + within;
    return Status::ok;
}

bool DescriptorTemplateLayout::invariants_hold() const
{
    AV_SCOPE("DescriptorTemplateLayout::invariants_hold");
    return entries_ <= max_entries && (entries_ != 0u || bytes_ == 0u);
}

}  // namespace av
