#include "icd/vulkan/descriptor_template_table.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/thread_lock.hpp"

namespace av::icd {

DescriptorTemplateTable::DescriptorTemplateTable(ThreadLock &lock) : lock_(lock), slots_{}
{
    AV_SCOPE("DescriptorTemplateTable::DescriptorTemplateTable");
}

VkResult DescriptorTemplateTable::record(VkDevice device, VkDescriptorUpdateTemplate made,
                                         const VkDescriptorUpdateTemplateCreateInfo *info)
{
    AV_SCOPE("DescriptorTemplateTable::record");
    if (device == VK_NULL_HANDLE || made == VK_NULL_HANDLE || info == nullptr)
        return VK_ERROR_INITIALIZATION_FAILED;
    DescriptorTemplateLayout layout;
    if (layout.take(info->pDescriptorUpdateEntries, info->descriptorUpdateEntryCount) != Status::ok)
        return VK_ERROR_INITIALIZATION_FAILED;
    lock_.acquire();
    ScopeExit released([&] { lock_.release(); });
    DescriptorTemplateSlot *free_slot = nullptr;
    for (DescriptorTemplateSlot &slot : slots_) {
        /* A handle a driver handed out again is this template now, not the one
         * that was destroyed at the same address. */
        if (slot.template_ == made) {
            free_slot = &slot;
            break;
        }
        if (slot.template_ == VK_NULL_HANDLE && free_slot == nullptr) free_slot = &slot;
    }
    if (free_slot == nullptr) return VK_ERROR_OUT_OF_HOST_MEMORY;
    free_slot->device_ = device;
    free_slot->template_ = made;
    free_slot->bytes_ = layout.bytes();
    return VK_SUCCESS;
}

void DescriptorTemplateTable::forget(VkDescriptorUpdateTemplate made)
{
    AV_SCOPE("DescriptorTemplateTable::forget");
    lock_.acquire();
    ScopeExit released([&] { lock_.release(); });
    for (DescriptorTemplateSlot &slot : slots_) {
        if (slot.template_ != made || made == VK_NULL_HANDLE) continue;
        slot = DescriptorTemplateSlot{};
        return;
    }
}

void DescriptorTemplateTable::forget_device(VkDevice device)
{
    AV_SCOPE("DescriptorTemplateTable::forget_device");
    lock_.acquire();
    ScopeExit released([&] { lock_.release(); });
    for (DescriptorTemplateSlot &slot : slots_)
        if (slot.device_ == device && device != VK_NULL_HANDLE) slot = DescriptorTemplateSlot{};
}

uint64_t DescriptorTemplateTable::bytes_of(VkDescriptorUpdateTemplate made) const
{
    AV_SCOPE("DescriptorTemplateTable::bytes_of");
    lock_.acquire();
    ScopeExit released([&] { lock_.release(); });
    const DescriptorTemplateSlot *const slot = slot_of(made);
    return slot != nullptr ? slot->bytes_ : 0u;
}

uint32_t DescriptorTemplateTable::live() const
{
    AV_SCOPE("DescriptorTemplateTable::live");
    lock_.acquire();
    ScopeExit released([&] { lock_.release(); });
    uint32_t found = 0u;
    for (const DescriptorTemplateSlot &slot : slots_) found += slot.template_ != VK_NULL_HANDLE ? 1u : 0u;
    return found;
}

const DescriptorTemplateSlot *DescriptorTemplateTable::slot_of(VkDescriptorUpdateTemplate made) const
{
    AV_SCOPE("DescriptorTemplateTable::slot_of");
    if (made == VK_NULL_HANDLE) return nullptr;
    for (const DescriptorTemplateSlot &slot : slots_)
        if (slot.template_ == made) return &slot;
    return nullptr;
}

bool DescriptorTemplateTable::invariants_hold() const
{
    AV_SCOPE("DescriptorTemplateTable::invariants_hold");
    for (uint32_t index = 0u; index < max_templates; index++) {
        const DescriptorTemplateSlot &slot = slots_[index];
        if (slot.template_ == VK_NULL_HANDLE) {
            if (slot.device_ != VK_NULL_HANDLE || slot.bytes_ != 0u) return false;
            continue;
        }
        if (slot.device_ == VK_NULL_HANDLE) return false;
        for (uint32_t earlier = 0u; earlier < index; earlier++)
            if (slots_[earlier].template_ == slot.template_) return false;
    }
    return true;
}

}  // namespace av::icd
