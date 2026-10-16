/* The chain the host is sent for an allocation made to be exported (feature
 * 047, US3, research.md §10; feature 053).
 *
 * A Win32 export cannot be passed on as it stands: the host's driver knows no
 * Win32 handle type, so `VkExportMemoryAllocateInfo` naming one would be
 * refused. This rebuilds the chain with the export said as the host's own
 * type, and carries the other nodes an allocation may hold as they are. An
 * import never comes this way: it is opened on the device (OpenedObjects), and
 * a chain naming one is refused here.
 *
 * A node it does not know is refused rather than dropped: silently losing a
 * dedicated allocation or an address flag would be memory the application
 * asked for and did not get. The Win32 export structure is dropped on purpose:
 * its security descriptor and name are what another process would open the
 * handle with, and the kernel's NT handle carries neither. */
#ifndef AV_ICD_MEMORY_ALLOCATE_CHAIN_HPP
#define AV_ICD_MEMORY_ALLOCATE_CHAIN_HPP

#include "core/support/status.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av::icd {

class Win32HandleKind;
struct MemoryAllocateChainProbe;

class MemoryAllocateChain {
public:
    /* The node types it knows: the export, the Win32 export structure it
     * drops, a dedicated allocation and the allocation flags. A chain may hold
     * each at most once, so a walk ends within this many whatever the chain
     * does - a cycle repeats a type and is refused. */
    static constexpr uint32_t carried_kinds = 4u;

    MemoryAllocateChain();
    MemoryAllocateChain(const MemoryAllocateChain &) = delete;
    MemoryAllocateChain &operator=(const MemoryAllocateChain &) = delete;

    /* The chain of `info`, with `kind`'s Win32 export said as `share_type`:
     * a dma-buf where the host has one and its opaque fd otherwise (feature
     * 053). not_supported for a node this does not carry - an import among
     * them - and for an export of any other handle type, malformed for the
     * same node twice. Unchanged unless it answers ok. */
    [[nodiscard]] Status take(const VkMemoryAllocateInfo &info, const Win32HandleKind &kind, VkFlags share_type);
    /* The allocation the host is sent, valid while this is. */
    const VkMemoryAllocateInfo *forwarded() const;
    bool                        exports() const;

    /* Every node in the chain it answers is one of its own, at most once, so
     * the chain ends. */
    bool invariants_hold() const;

private:
    /* Appends `node` to the chain being built. */
    void   link(void *node);
    void   clear();
    /* `why`, with nothing held. */
    Status refusal(Status why);

    VkMemoryAllocateInfo          info_;
    VkExportMemoryAllocateInfo    export_;
    VkMemoryDedicatedAllocateInfo dedicated_;
    VkMemoryAllocateFlagsInfo     flags_;
    VkBaseOutStructure           *last_;
    bool                          exports_;

    friend struct MemoryAllocateChainProbe;
};

}  // namespace av::icd

#endif  // AV_ICD_MEMORY_ALLOCATE_CHAIN_HPP
