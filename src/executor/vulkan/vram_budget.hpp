#ifndef AV_VRAM_BUDGET_HPP
#define AV_VRAM_BUDGET_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "executor/vram_account.hpp"

#include <stdint.h>

#include <vulkan/vulkan.h>

namespace av {

class BlockSource;
class ByteLedger;

class VramBudget {
public:
    explicit VramBudget(BlockSource &blocks);
    VramBudget(const VramBudget &) = delete;
    VramBudget &operator=(const VramBudget &) = delete;

    void     bind(ByteLedger *vram, ByteLedger *host);
    void     learn(const VkPhysicalDeviceMemoryProperties &host);
    void     clamp(VkPhysicalDeviceMemoryProperties &answer) const;
    void     report(VkPhysicalDeviceMemoryBudgetPropertiesEXT &budget) const;
    [[nodiscard]] bool admit(uint32_t heap, uint64_t size);
    void     refund(uint32_t heap, uint64_t size);
    bool     system(uint32_t heap) const;
    [[nodiscard]] Status commit(uint32_t heap, uint64_t memory, uint64_t size);
    void     release(uint64_t memory);
    void     close();
    uint64_t used(uint32_t heap) const;
    uint64_t held() const;
    uint64_t host_held() const;

    bool invariants_hold() const;

private:
    struct Allocation {
        uint64_t size_;
        uint32_t heap_;
    };

    bool capped(uint32_t heap) const;

    VkPhysicalDeviceMemoryProperties host_;
    VramAccount                      account_;
    VramAccount                      host_account_;
    uint64_t                         used_[VK_MAX_MEMORY_HEAPS];
    KeyedSlots<uint64_t, Allocation> allocations_;
};

}

#endif
