#ifndef AV_ICD_MEMORY_TYPE_TABLE_HPP
#define AV_ICD_MEMORY_TYPE_TABLE_HPP

#include "core/support/status.hpp"
#include "guest/kmt/memory_segment_group.hpp"
#include "icd/common/adapter_memory.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class Kmt;
}

namespace av::icd {

struct MemoryTypeTableProbe;

class MemoryTypeTable {
public:
    MemoryTypeTable(Kmt &kmt, uint32_t adapter);
    MemoryTypeTable(const MemoryTypeTable &) = delete;
    MemoryTypeTable &operator=(const MemoryTypeTable &) = delete;

    [[nodiscard]] Status build(const VkPhysicalDeviceMemoryProperties &host);
    void                 properties(VkPhysicalDeviceMemoryProperties &answer) const;
    void                 budget(VkPhysicalDeviceMemoryBudgetPropertiesEXT &answer,
                                const VkPhysicalDeviceMemoryBudgetPropertiesEXT &hosted) const;
    bool                 host_visible(uint32_t type) const;
    bool                 placed(uint32_t type) const;
    uint32_t             lendable_types() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] bool acceptable(const VkPhysicalDeviceMemoryProperties &host) const;
    MemorySegmentGroup group_of(const VkMemoryHeap &heap) const;
    void               clear();

    AdapterMemory                    memory_;
    VkPhysicalDeviceMemoryProperties properties_;
    uint64_t                         shared_budget_;
    uint32_t                         placed_;

    friend struct MemoryTypeTableProbe;
};

}

#endif
