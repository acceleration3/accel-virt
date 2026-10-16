#include "icd/vulkan/external_objects.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"

namespace av::icd {

ExternalObjects::ExternalObjects(ThreadLock &lock, BlockSource &memory) : lock_(lock), slots_(memory)
{
    AV_SCOPE("ExternalObjects::ExternalObjects");
}

ExternalObjectSlot *ExternalObjects::find(VkDeviceMemory memory, VkSemaphore semaphore)
{
    AV_SCOPE("ExternalObjects::find");
    return slots_.find_if([memory, semaphore](const ExternalObjectSlot &slot) {
        return (memory != VK_NULL_HANDLE && slot.memory_ == memory) ||
               (semaphore != VK_NULL_HANDLE && slot.semaphore_ == semaphore);
    });
}

const ExternalObjectSlot *ExternalObjects::find(VkDeviceMemory memory, VkSemaphore semaphore) const
{
    AV_SCOPE("ExternalObjects::find");
    return slots_.find_if([memory, semaphore](const ExternalObjectSlot &slot) {
        return (memory != VK_NULL_HANDLE && slot.memory_ == memory) ||
               (semaphore != VK_NULL_HANDLE && slot.semaphore_ == semaphore);
    });
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
    const ThreadLockHold released(lock_);
    if (find(memory, semaphore) != nullptr) return false;
    ExternalObjectSlot *row = nullptr;
    return slots_.create(row, ExternalObjectSlot{device, memory, semaphore, object_id, size, handle, carrier}) ==
           Status::ok;
}

uint64_t ExternalObjects::handle_of_memory(VkDeviceMemory memory) const
{
    AV_SCOPE("ExternalObjects::handle_of_memory");
    if (memory == VK_NULL_HANDLE) return 0u;
    const ThreadLockHold released(lock_);
    const ExternalObjectSlot *const slot = find(memory, VK_NULL_HANDLE);
    return slot != nullptr ? slot->handle_ : 0u;
}

uint64_t ExternalObjects::handle_of_semaphore(VkSemaphore semaphore) const
{
    AV_SCOPE("ExternalObjects::handle_of_semaphore");
    if (semaphore == VK_NULL_HANDLE) return 0u;
    const ThreadLockHold released(lock_);
    const ExternalObjectSlot *const slot = find(VK_NULL_HANDLE, semaphore);
    return slot != nullptr ? slot->handle_ : 0u;
}

bool ExternalObjects::holder_of(uint64_t object_id, ExternalObjectSlot &row) const
{
    AV_SCOPE("ExternalObjects::holder_of");
    if (object_id == 0u) return false;
    const ThreadLockHold             released(lock_);
    const ExternalObjectSlot *const slot =
        slots_.find_if([object_id](const ExternalObjectSlot &held) { return held.object_id_ == object_id; });
    if (slot == nullptr) return false;
    row = *slot;
    return true;
}

void ExternalObjects::give_back(ExternalObjectSlot &slot, ExternalObjectSlot &row)
{
    AV_SCOPE("ExternalObjects::give_back");
    row = slot;
    static_cast<void>(slots_.destroy(&slot));
}

bool ExternalObjects::take_memory(VkDeviceMemory memory, ExternalObjectSlot &row)
{
    AV_SCOPE("ExternalObjects::take_memory");
    if (memory == VK_NULL_HANDLE) return false;
    const ThreadLockHold released(lock_);
    ExternalObjectSlot *const slot = find(memory, VK_NULL_HANDLE);
    if (slot == nullptr) return false;
    give_back(*slot, row);
    return true;
}

bool ExternalObjects::take_semaphore(VkSemaphore semaphore, ExternalObjectSlot &row)
{
    AV_SCOPE("ExternalObjects::take_semaphore");
    if (semaphore == VK_NULL_HANDLE) return false;
    const ThreadLockHold released(lock_);
    ExternalObjectSlot *const slot = find(VK_NULL_HANDLE, semaphore);
    if (slot == nullptr) return false;
    give_back(*slot, row);
    return true;
}

bool ExternalObjects::take_any_of_device(VkDevice device, ExternalObjectSlot &row)
{
    AV_SCOPE("ExternalObjects::take_any_of_device");
    if (device == VK_NULL_HANDLE) return false;
    const ThreadLockHold released(lock_);
    ExternalObjectSlot *const slot =
        slots_.find_if([device](const ExternalObjectSlot &held) { return held.device_ == device; });
    if (slot == nullptr) return false;
    give_back(*slot, row);
    return true;
}

uint32_t ExternalObjects::live() const
{
    AV_SCOPE("ExternalObjects::live");
    const ThreadLockHold released(lock_);
    return slots_.live();
}

bool ExternalObjects::invariants_hold() const
{
    AV_SCOPE("ExternalObjects::invariants_hold");
    return slots_.all_of([this](const ExternalObjectSlot &slot) {
        const bool memory = slot.memory_ != VK_NULL_HANDLE;
        const bool semaphore = slot.semaphore_ != VK_NULL_HANDLE;
        if (slot.device_ == VK_NULL_HANDLE || memory == semaphore) return false;
        if (slot.object_id_ == 0u || slot.handle_ == 0u) return false;
        if (memory != (slot.size_ != 0u)) return false;
        uint32_t alike = 0u;
        slots_.for_each([&slot, &alike](const ExternalObjectSlot &other) {
            if (other.object_id_ == slot.object_id_ || other.handle_ == slot.handle_ ||
                (other.memory_ == slot.memory_ && slot.memory_ != VK_NULL_HANDLE) ||
                (other.semaphore_ == slot.semaphore_ && slot.semaphore_ != VK_NULL_HANDLE))
                alike++;
        });
        return alike == 1u;
    });
}

}  // namespace av::icd
