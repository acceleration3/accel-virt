/* The memory one device allocates (docs/host-memory.md). A host-visible type
 * is host GPU memory of exactly that type, one allocation per vkAllocateMemory:
 * opened in the host window, imported into the device's context as the
 * VkDeviceMemory the host answers, and mapped by being this process's own view
 * of it with no round trip. Any other type is the host's to allocate, and
 * cannot be mapped. The types are learnt from the host the first time the
 * device allocates, since a device that never allocates never needs them.
 * Callers serialize it: IcdRoot holds its memory lock around every call. */
#ifndef AV_ICD_DEVICE_MEMORY_HPP
#define AV_ICD_DEVICE_MEMORY_HPP

#include "core/log/driver_log.hpp"
#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "icd/vulkan/memory_type_table.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class BlockSource;
class HostAllocation;
class Kmt;
class LogSink;
class StateWriter;
}  // namespace av

namespace av::icd {

class Connection;
struct DeviceMemoryProbe;

class DeviceMemory {
public:
    /* Its allocations are kept in what grows from `blocks`: as many as the
     * application makes. */
    DeviceMemory(Kmt &kmt, Connection &connection, LogSink &log, BlockSource &blocks);
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

    /* Whether the host's memory types were learnt, and the host allocations
     * held: plain counters, read as they stand. */
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
    /* Keeps `made`, in the room made for it, and answers its handle. */
    VkResult           hold(HostAllocation &made, VkDeviceMemory &memory);
    /* Room to keep one more: what is asked before the host opens any. */
    [[nodiscard]] bool room();
    [[nodiscard]] bool learn(VkPhysicalDevice physical);
    VkResult           refusal() const;
    /* The host allocation the guest names `memory`, or null. */
    HostAllocation    *find(VkDeviceMemory memory) const;

    Connection     &connection_;
    LogSink        &log_;
    DriverLog       lines_;
    MemoryTypeTable types_;
    bool            learnt_;
    /* This device's host allocations, by the handle the host answered, which
     * the connection's channel keeps: one channel serves every device of an
     * instance. */
    KeyedSlots<uint64_t, HostAllocation *> held_;

    friend struct DeviceMemoryProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_DEVICE_MEMORY_HPP
