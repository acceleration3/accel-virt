#ifndef AV_VRAM_BUDGET_HPP
#define AV_VRAM_BUDGET_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

#include <vulkan/vulkan.h>

namespace av {

class BlockSource;

class VramBudget {
public:
    static constexpr uint64_t headroom_floor = 1ull << 30;
    static constexpr uint64_t headroom_divisor = 8u;

    explicit VramBudget(BlockSource &blocks);
    VramBudget(const VramBudget &) = delete;
    VramBudget &operator=(const VramBudget &) = delete;

    void     learn(const VkPhysicalDeviceMemoryProperties &host);
    uint64_t cap_for(uint64_t total) const;
    void     clamp(VkPhysicalDeviceMemoryProperties &answer) const;
    void     report(VkPhysicalDeviceMemoryBudgetPropertiesEXT &budget) const;
    bool     fits(uint32_t heap, uint64_t size) const;
    [[nodiscard]] Status commit(uint32_t heap, uint64_t memory, uint64_t size);
    void     release(uint64_t memory);
    uint64_t used(uint32_t heap) const;

    bool invariants_hold() const;

private:
    struct Allocation {
        uint64_t size_;
        uint32_t heap_;
    };

    bool capped(uint32_t heap) const;

    VkPhysicalDeviceMemoryProperties host_;
    uint64_t                         caps_[VK_MAX_MEMORY_HEAPS];
    uint64_t                         used_[VK_MAX_MEMORY_HEAPS];
    KeyedSlots<uint64_t, Allocation> allocations_;
};

}

#endif
