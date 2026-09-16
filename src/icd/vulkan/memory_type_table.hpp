/* The memory types and heaps one physical device answers the guest with
 * (research.md §7). The types are the host's as the executor narrowed them:
 * host visible only where the host GPU imports a host pointer, since the only
 * memory a guest process can map is its own RAM (VulkanBackend,
 * ImportableTypes). None is ever added. The heaps are the adapter's: a
 * device-local heap is the adapter's local budget, any other its non-local
 * one, as D3DKMTQueryVideoMemoryInfo answers them for this process.
 *
 * The host's answer is its word, so a count past the arrays, a device with
 * types and no heap, or a type on a heap past the heaps is refused and the
 * table left empty. */
#ifndef AV_ICD_MEMORY_TYPE_TABLE_HPP
#define AV_ICD_MEMORY_TYPE_TABLE_HPP

#include "core/support/status.hpp"

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

    /* The adapter's budgets, read now, over `host`'s types: the Kmt's refusal,
     * malformed for a host whose arrays do not hold together, and nothing
     * built unless ok. */
    [[nodiscard]] Status build(const VkPhysicalDeviceMemoryProperties &host);
    /* The whole structure, arrays cleared past the counts. */
    void                 properties(VkPhysicalDeviceMemoryProperties &answer) const;
    /* VK_EXT_memory_budget's arrays; the structure's sType and pNext are left. */
    void                 budget(VkPhysicalDeviceMemoryBudgetPropertiesEXT &answer) const;
    /* Whether `type` exists and is one the ICD maps. */
    bool                 host_visible(uint32_t type) const;

    /* Counts inside the arrays, a heap for every type, each type on one of them,
     * and each heap the size of its group's budget. */
    bool invariants_hold() const;

private:
    static constexpr uint32_t groups = 2u;

    [[nodiscard]] bool acceptable(const VkPhysicalDeviceMemoryProperties &host) const;
    uint32_t           group_of(const VkMemoryHeap &heap) const;
    void               clear();

    Kmt                             &kmt_;
    uint32_t                         adapter_;
    VkPhysicalDeviceMemoryProperties properties_;
    uint64_t                         budgets_[groups];
    uint64_t                         usages_[groups];

    friend struct MemoryTypeTableProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_MEMORY_TYPE_TABLE_HPP
