/* The memory one device allocates (docs/host-memory.md). A host-visible type
 * is host GPU memory of exactly that type, one allocation per vkAllocateMemory:
 * opened in the host window, imported into the device's context as the
 * VkDeviceMemory the host answers, and mapped by being this process's own view
 * of it with no round trip. Any other type is the host's to allocate, and
 * cannot be mapped. The types are learnt from the host the first time the
 * device allocates, since a device that never allocates never needs them.
 * Its host allocations are kept by the instance's channel, which one
 * connection's devices share, for this device (HostChannel::open_memory).
 * Callers serialize it: IcdRoot holds its memory lock around every call. */
#ifndef AV_ICD_DEVICE_MEMORY_HPP
#define AV_ICD_DEVICE_MEMORY_HPP

#include "core/support/status.hpp"
#include "icd/vulkan/memory_type_table.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class HostAllocation;
class Kmt;
class DriverLog;
class StateWriter;
}  // namespace av

namespace av::vkproto {
class HandleTranslation;
}  // namespace av::vkproto

namespace av::icd {

class Connection;
struct DeviceMemoryProbe;

class DeviceMemory {
public:
    DeviceMemory(Kmt &kmt, Connection &connection, vkproto::HandleTranslation &handles, DriverLog &log);
    DeviceMemory(const DeviceMemory &) = delete;
    DeviceMemory &operator=(const DeviceMemory &) = delete;

    /* A host-visible type as host memory of that type, answering
     * VK_ERROR_OUT_OF_DEVICE_MEMORY for any refusal, or when the types cannot
     * be learnt from `physical` - VK_ERROR_DEVICE_LOST when the host faulted
     * the connection meanwhile, VK_ERROR_OUT_OF_HOST_MEMORY when there is no
     * memory here to keep it in; any other type as the host answers it. */
    VkResult allocate(VkPhysicalDevice physical, VkDevice device, const VkMemoryAllocateInfo &info,
                      const VkAllocationCallbacks *allocator, VkDeviceMemory &memory);
    /* vkGetMemoryHostPointerPropertiesEXT: the types a host allocation's
     * pages can be imported as (MemoryTypeTable::lendable_types).
     * VK_ERROR_INVALID_EXTERNAL_HANDLE for another handle type or a pointer
     * off the import alignment. */
    VkResult host_pointer_types(VkPhysicalDevice physical, VkExternalMemoryHandleTypeFlagBits type,
                                const void *pointer, uint32_t &types);
    /* Host memory is released; anything else is the host's to free. */
    void     free(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *allocator);
    /* The view at `offset`, with no host call. VK_ERROR_MEMORY_MAP_FAILED for
     * memory that is not host memory, flags, no size, and a range past the
     * allocation. */
    VkResult map(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags,
                 void *&data) const;
    /* Every host allocation released, before the device goes: the first
     * refusal. */
    [[nodiscard]] Status release_all();

    /* Whether the host's memory types were learnt. */
    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    /* minImportedHostPointerAlignment: what the device lends, a page. */
    static constexpr uintptr_t host_pointer_alignment = 4096u;

    /* The application's pages `import` names, lent to the device as host
     * memory of the type `info` asks (docs/host-memory.md, "Application
     * memory"). */
    VkResult           lend(const VkMemoryAllocateInfo &info, const VkImportMemoryHostPointerInfoEXT &import,
                            VkDeviceMemory &memory);
    /* The answer to an allocation the channel refused with `refused`. */
    VkResult           refusal(Status refused) const;
    [[nodiscard]] bool learn(VkPhysicalDevice physical);
    VkResult           refusal() const;
    /* This device's host allocation the guest names `memory`, or null. */
    HostAllocation    *find(VkDeviceMemory memory) const;

    Connection     &connection_;
    vkproto::HandleTranslation &handles_;
    DriverLog      &lines_;
    MemoryTypeTable types_;
    bool            learnt_;

    friend struct DeviceMemoryProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_DEVICE_MEMORY_HPP
