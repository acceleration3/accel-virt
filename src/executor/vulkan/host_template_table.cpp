#include "executor/vulkan/host_template_table.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

HostTemplateTable::HostTemplateTable() : slots_{}
{
    AV_SCOPE("HostTemplateTable::HostTemplateTable");
}

Status HostTemplateTable::reserve(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *info)
{
    AV_SCOPE("HostTemplateTable::reserve");
    if (device == VK_NULL_HANDLE || info == nullptr) return Status::invalid_parameter;
    DescriptorTemplateLayout layout;
    if (layout.take(info->pDescriptorUpdateEntries, info->descriptorUpdateEntryCount) != Status::ok)
        return Status::malformed;
    HostTemplateSlot *const free_slot = reserved();
    if (free_slot == nullptr) return Status::no_space;
    free_slot->device_ = device;
    free_slot->template_ = VK_NULL_HANDLE;
    free_slot->bytes_ = layout.bytes();
    free_slot->reserved_ = true;
    return Status::ok;
}

void HostTemplateTable::name(VkDescriptorUpdateTemplate made)
{
    AV_SCOPE("HostTemplateTable::name");
    for (HostTemplateSlot &slot : slots_) {
        if (!slot.reserved_) continue;
        /* The driver answered no handle: the slot was never a template. */
        if (made == VK_NULL_HANDLE) {
            slot = HostTemplateSlot{};
            return;
        }
        /* A handle the driver handed out again is this template now, so the
         * slot that held it before is not two templates. */
        for (HostTemplateSlot &earlier : slots_)
            if (&earlier != &slot && earlier.template_ == made) earlier = HostTemplateSlot{};
        slot.template_ = made;
        slot.reserved_ = false;
        return;
    }
}

void HostTemplateTable::forget(VkDescriptorUpdateTemplate made)
{
    AV_SCOPE("HostTemplateTable::forget");
    for (HostTemplateSlot &slot : slots_) {
        if (slot.template_ != made || made == VK_NULL_HANDLE) continue;
        slot = HostTemplateSlot{};
        return;
    }
}

void HostTemplateTable::forget_device(VkDevice device)
{
    AV_SCOPE("HostTemplateTable::forget_device");
    for (HostTemplateSlot &slot : slots_)
        if (slot.device_ == device && device != VK_NULL_HANDLE) slot = HostTemplateSlot{};
}

void HostTemplateTable::forget_all()
{
    AV_SCOPE("HostTemplateTable::forget_all");
    for (HostTemplateSlot &slot : slots_) slot = HostTemplateSlot{};
}

uint64_t HostTemplateTable::bytes_of(VkDescriptorUpdateTemplate made) const
{
    AV_SCOPE("HostTemplateTable::bytes_of");
    const HostTemplateSlot *const slot = slot_of(made);
    return slot != nullptr ? slot->bytes_ : 0u;
}

uint32_t HostTemplateTable::live() const
{
    AV_SCOPE("HostTemplateTable::live");
    uint32_t found = 0u;
    for (const HostTemplateSlot &slot : slots_) found += slot.template_ != VK_NULL_HANDLE ? 1u : 0u;
    return found;
}

/* The slot `reserve` takes: one already reserved, so a create that follows a
 * create the driver never answered reuses it, or the first empty one. */
HostTemplateSlot *HostTemplateTable::reserved()
{
    AV_SCOPE("HostTemplateTable::reserved");
    HostTemplateSlot *empty = nullptr;
    for (HostTemplateSlot &slot : slots_) {
        if (slot.reserved_) return &slot;
        if (slot.template_ == VK_NULL_HANDLE && empty == nullptr) empty = &slot;
    }
    return empty;
}

const HostTemplateSlot *HostTemplateTable::slot_of(VkDescriptorUpdateTemplate made) const
{
    AV_SCOPE("HostTemplateTable::slot_of");
    if (made == VK_NULL_HANDLE) return nullptr;
    for (const HostTemplateSlot &slot : slots_)
        if (slot.template_ == made) return &slot;
    return nullptr;
}

bool HostTemplateTable::invariants_hold() const
{
    AV_SCOPE("HostTemplateTable::invariants_hold");
    uint32_t reservations = 0u;
    for (uint32_t index = 0u; index < max_templates; index++) {
        const HostTemplateSlot &slot = slots_[index];
        if (slot.template_ == VK_NULL_HANDLE) {
            /* Reserved is a device and a size with no handle yet; empty is
             * nothing at all. */
            if (slot.reserved_ != (slot.device_ != VK_NULL_HANDLE)) return false;
            if (!slot.reserved_ && slot.bytes_ != 0u) return false;
            reservations += slot.reserved_ ? 1u : 0u;
            continue;
        }
        if (slot.device_ == VK_NULL_HANDLE || slot.reserved_) return false;
        for (uint32_t earlier = 0u; earlier < index; earlier++)
            if (slots_[earlier].template_ == slot.template_) return false;
    }
    return reservations <= 1u;
}

}  // namespace av
