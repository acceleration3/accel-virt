#include "icd/vulkan/descriptor_template_table.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"

namespace av::icd {

DescriptorTemplateTable::DescriptorTemplateTable(BlockSource &memory, ThreadLock &lock)
    : lock_(lock), slots_(memory, Pool::paged)
{
    AV_SCOPE("DescriptorTemplateTable::DescriptorTemplateTable");
}

/* A non-dispatchable handle is a pointer on 64-bit Windows and a 64-bit
 * integer on 32-bit, so its bytes are what is compared. */
uint64_t DescriptorTemplateTable::key_of(VkDescriptorUpdateTemplate made) const
{
    AV_SCOPE("DescriptorTemplateTable::key_of");
    uint64_t key = 0u;
    __builtin_memcpy(&key, &made, sizeof made);
    return key;
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
    const ThreadLockHold         released(lock_);
    const DescriptorTemplateSlot recorded{device, made, layout.bytes()};
    /* A handle a driver handed out again is this template now, not the one
     * that was destroyed at the same address. */
    DescriptorTemplateSlot *const known = slots_.find(key_of(made));
    if (known != nullptr) {
        *known = recorded;
        return VK_SUCCESS;
    }
    uint32_t slot = 0u;
    return slots_.insert(key_of(made), recorded, slot) == Status::ok ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY;
}

void DescriptorTemplateTable::forget(VkDescriptorUpdateTemplate made)
{
    AV_SCOPE("DescriptorTemplateTable::forget");
    const ThreadLockHold released(lock_);
    if (made != VK_NULL_HANDLE) static_cast<void>(slots_.erase(key_of(made)));
}

void DescriptorTemplateTable::forget_device(VkDevice device)
{
    AV_SCOPE("DescriptorTemplateTable::forget_device");
    const ThreadLockHold released(lock_);
    slots_.erase_if([device](uint64_t, DescriptorTemplateSlot &slot) { return slot.device_ == device; });
}

uint64_t DescriptorTemplateTable::bytes_of(VkDescriptorUpdateTemplate made) const
{
    AV_SCOPE("DescriptorTemplateTable::bytes_of");
    const ThreadLockHold released(lock_);
    const DescriptorTemplateSlot *const slot = made != VK_NULL_HANDLE ? slots_.find(key_of(made)) : nullptr;
    return slot != nullptr ? slot->bytes_ : 0u;
}

uint32_t DescriptorTemplateTable::live() const
{
    AV_SCOPE("DescriptorTemplateTable::live");
    const ThreadLockHold released(lock_);
    return slots_.count();
}

bool DescriptorTemplateTable::invariants_hold() const
{
    AV_SCOPE("DescriptorTemplateTable::invariants_hold");
    bool named = true;
    slots_.for_each([this, &named](uint64_t key, const DescriptorTemplateSlot &slot) {
        named = named && slot.device_ != VK_NULL_HANDLE && key_of(slot.template_) == key;
    });
    return named && slots_.invariants_hold();
}

}  // namespace av::icd
