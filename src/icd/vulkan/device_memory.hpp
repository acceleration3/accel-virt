/* The memory one device allocates (research.md §7, contracts/host_visible_memory.md
 * §4). A host-visible type is served from chunks of this process's RAM the host
 * GPU imports, so mapping it is the process's own view with no round trip; any
 * other type is the host's to allocate, and cannot be mapped. The types are
 * learnt from the host the first time the device allocates, since a device
 * that never allocates never needs them. Callers serialize it: IcdRoot holds its
 * memory lock around every call. */
#ifndef AV_ICD_DEVICE_MEMORY_HPP
#define AV_ICD_DEVICE_MEMORY_HPP

#include "core/support/status.hpp"
#include "icd/vulkan/chunk_allocator.hpp"
#include "icd/vulkan/memory_type_table.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class BlockSource;
class Kmt;
}  // namespace av

namespace av::icd {

class Connection;
class LogSink;
struct DeviceMemoryProbe;

class DeviceMemory {
public:
    DeviceMemory(BlockSource &memory, Kmt &kmt, Connection &connection, LogSink &log);
    DeviceMemory(const DeviceMemory &) = delete;
    DeviceMemory &operator=(const DeviceMemory &) = delete;

    /* A host-visible type from a chunk, answering VK_ERROR_OUT_OF_DEVICE_MEMORY
     * for any refusal, or when the types cannot be learnt from `physical`; any
     * other type as the host answers it. */
    VkResult allocate(VkPhysicalDevice physical, VkDevice device, const VkMemoryAllocateInfo &info,
                      const VkAllocationCallbacks *allocator, VkDeviceMemory &memory);
    /* A chunk's allocation is released; anything else is the host's to free. */
    void     free(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *allocator);
    /* The chunk's view at `offset`, with no host call. VK_ERROR_MEMORY_MAP_FAILED for
     * memory not from a chunk, flags, no size, and a range past the
     * allocation. */
    VkResult map(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags,
                 void *&data) const;
    /* Every chunk allocation released, before the device goes. */
    [[nodiscard]] Status release_all();

    bool invariants_hold() const;

private:
    [[nodiscard]] bool learn(VkPhysicalDevice physical);
    void               note(const char *what, uint64_t value);

    Connection     &connection_;
    LogSink        &log_;
    MemoryTypeTable types_;
    bool            learnt_;
    ChunkAllocator  chunks_;

    friend struct DeviceMemoryProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_DEVICE_MEMORY_HPP
