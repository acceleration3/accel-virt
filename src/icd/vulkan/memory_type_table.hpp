/* The memory types and heaps one physical device answers the guest with
 * (research.md §7). The types are the host's as the executor narrowed them:
 * host visible only where the host GPU imports memory it did not allocate,
 * since every mappable chunk reaches it as an import (VulkanBackend,
 * ImportableTypes) - a host window chunk as much as guest RAM. None is ever
 * added, and none is changed: a window chunk is mapped write-back on both
 * sides, which is what the host's HOST_CACHED types promise, and a chunk
 * that fell back to guest RAM is the kernel's lock of it, coherent either
 * way (docs/host-window.md). A device-local heap is the host's own size:
 * what lives there is host GPU memory the guest never maps, which the
 * adapter's local segment only names to VidMm. Any other heap is the
 * adapter's non-local budget, as D3DKMTQueryVideoMemoryInfo answers it for
 * this process.
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
     * and each heap that is not device-local the size of the non-local budget. */
    bool invariants_hold() const;

private:
    static constexpr uint32_t groups = 2u;

    [[nodiscard]] bool acceptable(const VkPhysicalDeviceMemoryProperties &host) const;
    uint32_t           group_of(const VkMemoryHeap &heap) const;
    void               clear();

    Kmt                             &kmt_;
    uint32_t                         adapter_;
    VkPhysicalDeviceMemoryProperties properties_;
    uint64_t                         shared_budget_;
    uint64_t                         usages_[groups];

    friend struct MemoryTypeTableProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_MEMORY_TYPE_TABLE_HPP
