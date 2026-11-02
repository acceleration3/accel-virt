#ifndef AV_ICD_EXTERNAL_OBJECTS_HPP
#define AV_ICD_EXTERNAL_OBJECTS_HPP

#include <stdint.h>

#include "core/support/object_pool.hpp"

#include <vulkan/vulkan_core.h>

namespace av::icd {

struct ExternalObjectsProbe;

}

namespace av {
class BlockSource;
class ThreadLock;
}

namespace av::icd {
struct ExternalObjectSlot {
    VkDevice     device_;
    VkDeviceMemory memory_;
    VkSemaphore    semaphore_;
    uint64_t       object_id_;
    uint64_t       size_;
    uint64_t       handle_;
    uint32_t       carrier_;
};

class ExternalObjects {
public:
    ExternalObjects(ThreadLock &lock, BlockSource &memory);
    ExternalObjects(const ExternalObjects &) = delete;
    ExternalObjects &operator=(const ExternalObjects &) = delete;

    [[nodiscard]] bool remember(VkDevice device, VkDeviceMemory memory, VkSemaphore semaphore,
                                uint64_t object_id, uint64_t size, uint64_t handle, uint32_t carrier);
    uint64_t           handle_of_memory(VkDeviceMemory memory) const;
    uint64_t           handle_of_semaphore(VkSemaphore semaphore) const;
    [[nodiscard]] bool holder_of(uint64_t object_id, ExternalObjectSlot &row) const;
    [[nodiscard]] bool take_memory(VkDeviceMemory memory, ExternalObjectSlot &row);
    [[nodiscard]] bool take_semaphore(VkSemaphore semaphore, ExternalObjectSlot &row);
    [[nodiscard]] bool take_any_of_device(VkDevice device, ExternalObjectSlot &row);

    uint32_t live() const;

    bool invariants_hold() const;

private:
    ExternalObjectSlot       *find(VkDeviceMemory memory, VkSemaphore semaphore);
    const ExternalObjectSlot *find(VkDeviceMemory memory, VkSemaphore semaphore) const;
    void                      give_back(ExternalObjectSlot &slot, ExternalObjectSlot &row);

    ThreadLock                             &lock_;
    mutable ObjectPool<ExternalObjectSlot> slots_;

    friend struct ExternalObjectsProbe;
};

}

#endif
