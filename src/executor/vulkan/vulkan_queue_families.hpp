#ifndef AV_VULKAN_QUEUE_FAMILIES_HPP
#define AV_VULKAN_QUEUE_FAMILIES_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlockSource;

class VulkanQueueFamilies {
public:
    explicit VulkanQueueFamilies(BlockSource &blocks);
    VulkanQueueFamilies(const VulkanQueueFamilies &) = delete;
    VulkanQueueFamilies &operator=(const VulkanQueueFamilies &) = delete;

    [[nodiscard]] Status record(uint64_t pool, uint32_t family_flags);
    void                 forget(uint64_t pool);
    void                 clear();
    uint32_t             flags_of(uint64_t pool) const;
    uint32_t             count() const;

    bool invariants_hold() const;

private:
    KeyedSlots<uint64_t, uint32_t> pools_;
};

}

#endif
