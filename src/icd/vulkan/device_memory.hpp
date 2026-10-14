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
#include "core/support/status.hpp"
#include "icd/vulkan/memory_type_table.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
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
    DeviceMemory(Kmt &kmt, Connection &connection, LogSink &log);
    DeviceMemory(const DeviceMemory &) = delete;
    DeviceMemory &operator=(const DeviceMemory &) = delete;

    /* A host-visible type as host memory of that type, answering
     * VK_ERROR_OUT_OF_DEVICE_MEMORY for any refusal, or when the types cannot
     * be learnt from `physical` - VK_ERROR_DEVICE_LOST when the host faulted
     * the connection meanwhile; any other type as the host answers it. */
    VkResult allocate(VkPhysicalDevice physical, VkDevice device, const VkMemoryAllocateInfo &info,
                      const VkAllocationCallbacks *allocator, VkDeviceMemory &memory);
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
    static constexpr uint32_t max_allocations = 4096u;

    [[nodiscard]] bool learn(VkPhysicalDevice physical);
    VkResult           refusal() const;
    /* The host allocation the guest names `memory`, or null. */
    HostAllocation    *find(VkDeviceMemory memory) const;
    /* The slot holding `allocation` - a free one for null - or max_allocations. */
    uint32_t           slot_of(const HostAllocation *allocation) const;

    Connection     &connection_;
    LogSink        &log_;
    DriverLog       lines_;
    MemoryTypeTable types_;
    bool            learnt_;
    /* This device's host allocations, which the connection's channel keeps:
     * one channel serves every device of an instance. */
    HostAllocation *held_[max_allocations];
    uint32_t        count_;

    friend struct DeviceMemoryProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_DEVICE_MEMORY_HPP
