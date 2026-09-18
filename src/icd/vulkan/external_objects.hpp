/* The Win32 handles this process exported, and the host objects they name
 * (feature 047, US3, contracts/external_objects.md).
 *
 * No host driver on Linux has VK_KHR_external_memory_win32 or
 * VK_KHR_external_semaphore_win32, and a HANDLE is not a thing the wire
 * carries, so a handle here is this ICD's own: an opaque value that this table
 * turns back into the object it was exported for. Within one process that is
 * the whole of what DXGI interop needs - two devices seeing one image, one
 * fence waking another. A handle from another process is refused until feature
 * 053 gives objects a device-global identity, and a named handle is refused
 * because a name is exactly what another process would open.
 *
 * The lock is its own: an export happens on the calling application's thread,
 * and nothing it does reaches back into the root. */
#ifndef AV_ICD_EXTERNAL_OBJECTS_HPP
#define AV_ICD_EXTERNAL_OBJECTS_HPP

#include "icd/vulkan/memory_win32_handle_record.hpp"
#include "icd/vulkan/semaphore_win32_handle_record.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av::icd {

class ThreadLock;
struct ExternalObjectsProbe;

/* One exported handle. A type of its own so the table's slots are complete
 * (tools/header_scan.py reads a class's members only to its first nested
 * type). */
struct ExternalObjectSlot {
    VkDevice     device_;
    /* Exactly one of the two is set; the other is null. */
    VkDeviceMemory memory_;
    VkSemaphore    semaphore_;
};

class ExternalObjects {
public:
    /* More than an application shares: DXGI interop is a handful of surfaces
     * and fences per device. */
    static constexpr uint32_t max_objects = 64u;

    explicit ExternalObjects(ThreadLock &lock);
    ExternalObjects(const ExternalObjects &) = delete;
    ExternalObjects &operator=(const ExternalObjects &) = delete;

    /* A handle naming `request`'s memory, exported for `device`. The same
     * memory exported twice answers the same handle, as Windows does for a
     * handle to one object. VK_ERROR_INVALID_EXTERNAL_HANDLE for a record that
     * is not one, a handle type this path does not answer, or no memory;
     * VK_ERROR_TOO_MANY_OBJECTS when every slot is taken. */
    VkResult export_memory(VkDevice device, const MemoryGetWin32HandleRecord *request, void **handle);
    /* The memory a handle names and the device it was exported for, or null
     * for a handle this process did not export, a named one, or a handle type
     * this path does not answer. `owner` is what a fresh fd of the memory is
     * asked of, since memory belongs to one device. */
    VkDeviceMemory imported_memory(const ImportMemoryWin32HandleRecord *request, VkDevice &owner) const;
    VkResult       export_semaphore(VkDevice device, const SemaphoreGetWin32HandleRecord *request, void **handle);
    VkSemaphore    imported_semaphore(const ImportSemaphoreWin32HandleRecord *request, VkDevice &owner) const;

    /* An object destroyed takes its handle with it: a handle that outlived
     * what it names would be one an import could still follow. */
    void     forget_memory(VkDeviceMemory memory);
    void     forget_semaphore(VkSemaphore semaphore);
    void     forget_device(VkDevice device);
    uint32_t live() const;

    /* A slot is empty or names exactly one object of one device, and no object
     * appears twice. */
    bool invariants_hold() const;

private:
    /* The slot `handle` is, or null when it is not one of this table's. */
    const ExternalObjectSlot *slot_of(void *handle) const;
    ExternalObjectSlot       *taken(VkDevice device, VkDeviceMemory memory, VkSemaphore semaphore);
    /* A handle is its slot's address, as a surface's is (SurfaceTable). */
    void                     *handle_of(const ExternalObjectSlot *slot) const;

    ThreadLock        &lock_;
    ExternalObjectSlot slots_[max_objects];

    friend struct ExternalObjectsProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_EXTERNAL_OBJECTS_HPP
