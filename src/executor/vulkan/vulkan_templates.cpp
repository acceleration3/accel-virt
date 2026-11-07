#include "executor/vulkan/vulkan_templates.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/foreign_enum.hpp"

namespace av {

namespace {

using vkproto::HandleKind;

uint64_t number(const void *handle)
{
    AV_SCOPE("number");
    return reinterpret_cast<uint64_t>(handle);
}

VulkanTemplates::Reference none()
{
    AV_SCOPE("none");
    return VulkanTemplates::Reference{HandleKind::device, 0u, false};
}

VulkanTemplates::Reference named_as(HandleKind kind, uint64_t handle)
{
    AV_SCOPE("named_as");
    return VulkanTemplates::Reference{kind, handle, true};
}

}

VulkanTemplates::VulkanTemplates(BlockSource &blocks) : blocks_(blocks), pool_(blocks), templates_(blocks, Pool::paged)
{
    AV_SCOPE("VulkanTemplates::VulkanTemplates");
}

VulkanTemplates::Entries::Entries(BlockSource &blocks, uint64_t device)
    : entries_(blocks, Pool::paged), immutable_(blocks, Pool::paged), device_(device)
{
    AV_SCOPE("VulkanTemplates::Entries::Entries");
}

bool VulkanTemplates::Entries::fixes(uint32_t binding) const
{
    AV_SCOPE("VulkanTemplates::Entries::fixes");
    for (uint32_t index = 0u; index < immutable_.size(); index++)
        if (immutable_.at(index) == binding) return true;
    return false;
}

Status VulkanTemplates::take(uint64_t made, uint64_t device, const VkDescriptorUpdateTemplateEntry *entries,
                             uint32_t count, const uint32_t *immutable, uint32_t immutable_count)
{
    AV_SCOPE("VulkanTemplates::take");
    if (made == 0u || (entries == nullptr && count != 0u) || (immutable == nullptr && immutable_count != 0u))
        return Status::invalid_parameter;
    forget(made);
    Entries *listed = nullptr;
    if (pool_.create(listed, blocks_, device) != Status::ok) return Status::no_memory;
    Status kept = listed->entries_.reserve(count);
    for (uint32_t index = 0u; index < count && kept == Status::ok; index++) kept = listed->entries_.append(entries[index]);
    for (uint32_t index = 0u; index < immutable_count && kept == Status::ok; index++)
        kept = listed->immutable_.append(immutable[index]);
    uint32_t slot = 0u;
    if (kept == Status::ok) kept = templates_.insert(made, listed, slot);
    if (kept != Status::ok) static_cast<void>(pool_.destroy(listed));
    return kept;
}

void VulkanTemplates::forget(uint64_t made)
{
    AV_SCOPE("VulkanTemplates::forget");
    Entries *listed = nullptr;
    if (templates_.take(made, listed)) static_cast<void>(pool_.destroy(listed));
}

void VulkanTemplates::forget_device(uint64_t device)
{
    AV_SCOPE("VulkanTemplates::forget_device");
    templates_.erase_if([&](uint64_t, Entries *&listed) {
        if (listed->device_ != device) return false;
        static_cast<void>(pool_.destroy(listed));
        return true;
    });
}

void VulkanTemplates::clear()
{
    AV_SCOPE("VulkanTemplates::clear");
    templates_.clear();
    pool_.destroy_all();
}

Status VulkanTemplates::describe(const VkDescriptorUpdateTemplateEntry &entry, uint32_t index, const void *data,
                                 uint64_t bytes, bool immutable_sampler, Descriptor &described) const
{
    AV_SCOPE("VulkanTemplates::describe");
    described = Descriptor{entry.dstBinding, entry.dstArrayElement + index, none(), none()};
    const int32_t type = foreign_enum(entry.descriptorType);
    uint64_t      size = 0u;
    switch (type) {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: size = sizeof(VkDescriptorImageInfo); break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: size = sizeof(VkBufferView); break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: size = sizeof(VkDescriptorBufferInfo); break;
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: size = sizeof(VkAccelerationStructureKHR); break;
    default: return Status::ok;
    }
    const uint64_t stride = entry.stride;
    const uint64_t offset = static_cast<uint64_t>(entry.offset);
    if (index != 0u && stride > (~uint64_t{0} - offset) / index) return Status::too_large;
    const uint64_t at = offset + stride * index;
    if (data == nullptr || at > bytes || size > bytes - at) return Status::invalid_parameter;
    const uint8_t *const read = static_cast<const uint8_t *>(data) + at;
    switch (type) {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: {
        VkDescriptorImageInfo info = {};
        __builtin_memcpy(&info, read, sizeof info);
        if (type == VK_DESCRIPTOR_TYPE_SAMPLER) {
            if (!immutable_sampler) described.first_ = named_as(HandleKind::sampler, number(info.sampler));
            return Status::ok;
        }
        described.first_ = named_as(HandleKind::image_view, number(info.imageView));
        if (type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && !immutable_sampler)
            described.second_ = named_as(HandleKind::sampler, number(info.sampler));
        return Status::ok;
    }
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
        VkBufferView view = VK_NULL_HANDLE;
        __builtin_memcpy(&view, read, sizeof view);
        described.first_ = named_as(HandleKind::buffer_view, number(view));
        return Status::ok;
    }
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: {
        VkAccelerationStructureKHR structure = VK_NULL_HANDLE;
        __builtin_memcpy(&structure, read, sizeof structure);
        described.first_ = named_as(HandleKind::acceleration_structure_khr, number(structure));
        return Status::ok;
    }
    default: {
        VkDescriptorBufferInfo info = {};
        __builtin_memcpy(&info, read, sizeof info);
        described.first_ = named_as(HandleKind::buffer, number(info.buffer));
        return Status::ok;
    }
    }
}

bool VulkanTemplates::known(uint64_t made) const
{
    AV_SCOPE("VulkanTemplates::known");
    return templates_.find(made) != nullptr;
}

uint32_t VulkanTemplates::count() const
{
    AV_SCOPE("VulkanTemplates::count");
    return templates_.count();
}

bool VulkanTemplates::invariants_hold() const
{
    AV_SCOPE("VulkanTemplates::invariants_hold");
    if (!pool_.invariants_hold() || !templates_.invariants_hold() || pool_.live() != templates_.count()) return false;
    bool sound = true;
    templates_.for_each([&sound](uint64_t, Entries *const &listed) {
        sound = sound && listed != nullptr && listed->entries_.invariants_hold() && listed->immutable_.invariants_hold();
    });
    return sound;
}

}
