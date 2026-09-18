#include "icd/vulkan/external_objects.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/thread_lock.hpp"
#include "proto/wire/foreign_enum.hpp"

namespace av::icd {

ExternalObjects::ExternalObjects(ThreadLock &lock) : lock_(lock), slots_{}
{
    AV_SCOPE("ExternalObjects::ExternalObjects");
}

VkResult ExternalObjects::export_memory(VkDevice device, const MemoryGetWin32HandleRecord *request, void **handle)
{
    AV_SCOPE("ExternalObjects::export_memory");
    if (request == nullptr || handle == nullptr || device == VK_NULL_HANDLE)
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    if (foreign_enum(request->type_) != static_cast<int32_t>(memory_get_win32_handle_info) ||
        request->handle_type_ != opaque_win32_memory || request->memory_ == VK_NULL_HANDLE)
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    lock_.acquire();
    ScopeExit                 released([&] { lock_.release(); });
    ExternalObjectSlot *const slot = taken(device, request->memory_, VK_NULL_HANDLE);
    if (slot == nullptr) return VK_ERROR_TOO_MANY_OBJECTS;
    *handle = handle_of(slot);
    return VK_SUCCESS;
}

VkResult ExternalObjects::export_semaphore(VkDevice device, const SemaphoreGetWin32HandleRecord *request,
                                           void **handle)
{
    AV_SCOPE("ExternalObjects::export_semaphore");
    if (request == nullptr || handle == nullptr || device == VK_NULL_HANDLE)
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    if (foreign_enum(request->type_) != static_cast<int32_t>(semaphore_get_win32_handle_info) ||
        request->handle_type_ != opaque_win32_semaphore || request->semaphore_ == VK_NULL_HANDLE)
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    lock_.acquire();
    ScopeExit                 released([&] { lock_.release(); });
    ExternalObjectSlot *const slot = taken(device, VK_NULL_HANDLE, request->semaphore_);
    if (slot == nullptr) return VK_ERROR_TOO_MANY_OBJECTS;
    *handle = handle_of(slot);
    return VK_SUCCESS;
}

VkDeviceMemory ExternalObjects::imported_memory(const ImportMemoryWin32HandleRecord *request, VkDevice &owner) const
{
    AV_SCOPE("ExternalObjects::imported_memory");
    owner = VK_NULL_HANDLE;
    if (request == nullptr || request->name_ != nullptr) return VK_NULL_HANDLE;
    if (foreign_enum(request->type_) != static_cast<int32_t>(import_memory_win32_handle_info) ||
        request->handle_type_ != opaque_win32_memory)
        return VK_NULL_HANDLE;
    lock_.acquire();
    ScopeExit                       released([&] { lock_.release(); });
    const ExternalObjectSlot *const slot = slot_of(request->handle_);
    /* A slot found holding the other kind of object names no memory, and so
     * names no device to ask for one either. */
    if (slot == nullptr || slot->memory_ == VK_NULL_HANDLE) return VK_NULL_HANDLE;
    owner = slot->device_;
    return slot->memory_;
}

VkSemaphore ExternalObjects::imported_semaphore(const ImportSemaphoreWin32HandleRecord *request,
                                                VkDevice &owner) const
{
    AV_SCOPE("ExternalObjects::imported_semaphore");
    owner = VK_NULL_HANDLE;
    if (request == nullptr || request->name_ != nullptr) return VK_NULL_HANDLE;
    if (foreign_enum(request->type_) != static_cast<int32_t>(import_semaphore_win32_handle_info) ||
        request->handle_type_ != opaque_win32_semaphore)
        return VK_NULL_HANDLE;
    lock_.acquire();
    ScopeExit                       released([&] { lock_.release(); });
    const ExternalObjectSlot *const slot = slot_of(request->handle_);
    if (slot == nullptr || slot->semaphore_ == VK_NULL_HANDLE) return VK_NULL_HANDLE;
    owner = slot->device_;
    return slot->semaphore_;
}

void ExternalObjects::forget_memory(VkDeviceMemory memory)
{
    AV_SCOPE("ExternalObjects::forget_memory");
    if (memory == VK_NULL_HANDLE) return;
    lock_.acquire();
    ScopeExit released([&] { lock_.release(); });
    for (ExternalObjectSlot &slot : slots_)
        if (slot.memory_ == memory) slot = ExternalObjectSlot{};
}

void ExternalObjects::forget_semaphore(VkSemaphore semaphore)
{
    AV_SCOPE("ExternalObjects::forget_semaphore");
    if (semaphore == VK_NULL_HANDLE) return;
    lock_.acquire();
    ScopeExit released([&] { lock_.release(); });
    for (ExternalObjectSlot &slot : slots_)
        if (slot.semaphore_ == semaphore) slot = ExternalObjectSlot{};
}

void ExternalObjects::forget_device(VkDevice device)
{
    AV_SCOPE("ExternalObjects::forget_device");
    if (device == VK_NULL_HANDLE) return;
    lock_.acquire();
    ScopeExit released([&] { lock_.release(); });
    for (ExternalObjectSlot &slot : slots_)
        if (slot.device_ == device) slot = ExternalObjectSlot{};
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

/* The slot the object is already in, or the first empty one: exporting the same
 * object twice is one handle, as a handle to one Windows object is. */
ExternalObjectSlot *ExternalObjects::taken(VkDevice device, VkDeviceMemory memory, VkSemaphore semaphore)
{
    AV_SCOPE("ExternalObjects::taken");
    ExternalObjectSlot *empty = nullptr;
    for (ExternalObjectSlot &slot : slots_) {
        const bool same = memory != VK_NULL_HANDLE ? slot.memory_ == memory : slot.semaphore_ == semaphore;
        if (slot.device_ != VK_NULL_HANDLE && same) return &slot;
        if (slot.device_ == VK_NULL_HANDLE && empty == nullptr) empty = &slot;
    }
    if (empty == nullptr) return nullptr;
    empty->device_ = device;
    empty->memory_ = memory;
    empty->semaphore_ = semaphore;
    return empty;
}

const ExternalObjectSlot *ExternalObjects::slot_of(void *handle) const
{
    AV_SCOPE("ExternalObjects::slot_of");
    for (const ExternalObjectSlot &slot : slots_) {
        if (slot.device_ == VK_NULL_HANDLE) continue;
        if (handle_of(&slot) == handle) return &slot;
    }
    return nullptr;
}

void *ExternalObjects::handle_of(const ExternalObjectSlot *slot) const
{
    AV_SCOPE("ExternalObjects::handle_of");
    return const_cast<ExternalObjectSlot *>(slot);
}

bool ExternalObjects::invariants_hold() const
{
    AV_SCOPE("ExternalObjects::invariants_hold");
    for (uint32_t index = 0u; index < max_objects; index++) {
        const ExternalObjectSlot &slot = slots_[index];
        if (slot.device_ == VK_NULL_HANDLE) {
            if (slot.memory_ != VK_NULL_HANDLE || slot.semaphore_ != VK_NULL_HANDLE) return false;
            continue;
        }
        /* Exactly one kind of object per slot. */
        const bool memory = slot.memory_ != VK_NULL_HANDLE;
        const bool semaphore = slot.semaphore_ != VK_NULL_HANDLE;
        if (memory == semaphore) return false;
        for (uint32_t earlier = 0u; earlier < index; earlier++) {
            if (memory && slots_[earlier].memory_ == slot.memory_) return false;
            if (semaphore && slots_[earlier].semaphore_ == slot.semaphore_) return false;
        }
    }
    return true;
}

}  // namespace av::icd
