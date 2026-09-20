#include "icd/vulkan/external_objects.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/thread_lock.hpp"

namespace av::icd {

ExternalObjects::ExternalObjects(ThreadLock &lock) : lock_(lock), slots_{}
{
    AV_SCOPE("ExternalObjects::ExternalObjects");
}

ExternalObjectSlot *ExternalObjects::find(VkDeviceMemory memory, VkSemaphore semaphore)
{
    AV_SCOPE("ExternalObjects::find");
    for (ExternalObjectSlot &slot : slots_) {
        if (slot.device_ == VK_NULL_HANDLE) continue;
        if (memory != VK_NULL_HANDLE && slot.memory_ == memory) return &slot;
        if (semaphore != VK_NULL_HANDLE && slot.semaphore_ == semaphore) return &slot;
    }
    return nullptr;
}

const ExternalObjectSlot *ExternalObjects::find(VkDeviceMemory memory, VkSemaphore semaphore) const
{
    AV_SCOPE("ExternalObjects::find");
    for (const ExternalObjectSlot &slot : slots_) {
        if (slot.device_ == VK_NULL_HANDLE) continue;
        if (memory != VK_NULL_HANDLE && slot.memory_ == memory) return &slot;
        if (semaphore != VK_NULL_HANDLE && slot.semaphore_ == semaphore) return &slot;
    }
    return nullptr;
}

bool ExternalObjects::remember(VkDevice device, VkDeviceMemory memory, VkSemaphore semaphore,
                               uint64_t object_id, uint64_t size, uint64_t handle, uint32_t carrier)
{
    AV_SCOPE("ExternalObjects::remember");
    if (device == VK_NULL_HANDLE || object_id == 0u || handle == 0u) return false;
    /* Exactly one kind, because a row is one object and the two are told apart
     * by which field is set; and each kind's own size rule, which is the wire
     * body's. */
    if ((memory == VK_NULL_HANDLE) == (semaphore == VK_NULL_HANDLE)) return false;
    if ((memory != VK_NULL_HANDLE) != (size != 0u)) return false;
    lock_.acquire();
    ScopeExit released([&] { lock_.release(); });
    if (find(memory, semaphore) != nullptr) return false;
    for (ExternalObjectSlot &slot : slots_) {
        if (slot.device_ != VK_NULL_HANDLE) continue;
        slot = ExternalObjectSlot{device, memory, semaphore, object_id, size, handle, carrier};
        return true;
    }
    return false;
}

uint64_t ExternalObjects::handle_of_memory(VkDeviceMemory memory) const
{
    AV_SCOPE("ExternalObjects::handle_of_memory");
    if (memory == VK_NULL_HANDLE) return 0u;
    lock_.acquire();
    ScopeExit                       released([&] { lock_.release(); });
    const ExternalObjectSlot *const slot = find(memory, VK_NULL_HANDLE);
    return slot != nullptr ? slot->handle_ : 0u;
}

uint64_t ExternalObjects::handle_of_semaphore(VkSemaphore semaphore) const
{
    AV_SCOPE("ExternalObjects::handle_of_semaphore");
    if (semaphore == VK_NULL_HANDLE) return 0u;
    lock_.acquire();
    ScopeExit                       released([&] { lock_.release(); });
    const ExternalObjectSlot *const slot = find(VK_NULL_HANDLE, semaphore);
    return slot != nullptr ? slot->handle_ : 0u;
}

bool ExternalObjects::holder_of(uint64_t object_id, ExternalObjectSlot &row) const
{
    AV_SCOPE("ExternalObjects::holder_of");
    if (object_id == 0u) return false;
    lock_.acquire();
    ScopeExit released([&] { lock_.release(); });
    for (const ExternalObjectSlot &slot : slots_) {
        if (slot.device_ == VK_NULL_HANDLE || slot.object_id_ != object_id) continue;
        row = slot;
        return true;
    }
    return false;
}

void ExternalObjects::give_back(ExternalObjectSlot &slot, ExternalObjectSlot &row)
{
    AV_SCOPE("ExternalObjects::give_back");
    row = slot;
    slot = ExternalObjectSlot{};
}

bool ExternalObjects::take_memory(VkDeviceMemory memory, ExternalObjectSlot &row)
{
    AV_SCOPE("ExternalObjects::take_memory");
    if (memory == VK_NULL_HANDLE) return false;
    lock_.acquire();
    ScopeExit                 released([&] { lock_.release(); });
    ExternalObjectSlot *const slot = find(memory, VK_NULL_HANDLE);
    if (slot == nullptr) return false;
    give_back(*slot, row);
    return true;
}

bool ExternalObjects::take_semaphore(VkSemaphore semaphore, ExternalObjectSlot &row)
{
    AV_SCOPE("ExternalObjects::take_semaphore");
    if (semaphore == VK_NULL_HANDLE) return false;
    lock_.acquire();
    ScopeExit                 released([&] { lock_.release(); });
    ExternalObjectSlot *const slot = find(VK_NULL_HANDLE, semaphore);
    if (slot == nullptr) return false;
    give_back(*slot, row);
    return true;
}

bool ExternalObjects::take_any_of_device(VkDevice device, ExternalObjectSlot &row)
{
    AV_SCOPE("ExternalObjects::take_any_of_device");
    if (device == VK_NULL_HANDLE) return false;
    lock_.acquire();
    ScopeExit released([&] { lock_.release(); });
    for (ExternalObjectSlot &slot : slots_) {
        if (slot.device_ != device) continue;
        give_back(slot, row);
        return true;
    }
    return false;
}

uint32_t ExternalObjects::live() const
{
    AV_SCOPE("ExternalObjects::live");
    lock_.acquire();
    ScopeExit released([&] { lock_.release(); });
    uint32_t found = 0u;
    for (const ExternalObjectSlot &slot : slots_) found += slot.device_ != VK_NULL_HANDLE ? 1u : 0u;
    return found;
}

bool ExternalObjects::invariants_hold() const
{
    AV_SCOPE("ExternalObjects::invariants_hold");
    for (uint32_t index = 0u; index < max_objects; index++) {
        const ExternalObjectSlot &slot = slots_[index];
        if (slot.device_ == VK_NULL_HANDLE) {
            if (slot.memory_ != VK_NULL_HANDLE || slot.semaphore_ != VK_NULL_HANDLE ||
                slot.object_id_ != 0u || slot.size_ != 0u || slot.handle_ != 0u || slot.carrier_ != 0u)
                return false;
            continue;
        }
        const bool memory = slot.memory_ != VK_NULL_HANDLE;
        const bool semaphore = slot.semaphore_ != VK_NULL_HANDLE;
        if (memory == semaphore) return false;
        if (slot.object_id_ == 0u || slot.handle_ == 0u) return false;
        if (memory != (slot.size_ != 0u)) return false;
        for (uint32_t earlier = 0u; earlier < index; earlier++) {
            if (memory && slots_[earlier].memory_ == slot.memory_) return false;
            if (semaphore && slots_[earlier].semaphore_ == slot.semaphore_) return false;
            if (slots_[earlier].object_id_ == slot.object_id_) return false;
            if (slots_[earlier].handle_ == slot.handle_) return false;
        }
    }
    return true;
}

}  // namespace av::icd
