/* The chain the host is sent for an allocation that shares memory (feature
 * 047, US3, research.md §10).
 *
 * A Win32 export or import cannot be passed on as it stands: the host's driver
 * knows no Win32 handle type, so `VkExportMemoryAllocateInfo` naming one would
 * be refused, and `VkImportMemoryWin32HandleInfoKHR` names a handle only this
 * ICD understands. This rebuilds the chain over the host's fd mechanism - the
 * export said as an opaque fd, the import as `VkImportMemoryFdInfoKHR` of a
 * fresh fd of the remembered object - and carries the other nodes an
 * allocation may hold as they are.
 *
 * It is built only for an allocation that shares, and a node it does not know
 * is refused rather than dropped: silently losing a dedicated allocation or an
 * address flag would be memory the application asked for and did not get. The
 * two Win32 structures are dropped on purpose: a security descriptor and a
 * name are what another process would open with, and both devices here are in
 * one. */
#ifndef AV_ICD_MEMORY_ALLOCATE_CHAIN_HPP
#define AV_ICD_MEMORY_ALLOCATE_CHAIN_HPP

#include "core/support/status.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av::icd {

class Win32HandleKind;

class MemoryAllocateChain {
public:
    /* The node types it knows: the Win32 export and import, the export
     * structure it drops, a dedicated allocation and the allocation flags. A
     * chain may hold each at most once, so a walk ends within this many
     * whatever the chain does - a cycle repeats a type and is refused. */
    static constexpr uint32_t carried_kinds = 5u;

    MemoryAllocateChain();
    MemoryAllocateChain(const MemoryAllocateChain &) = delete;
    MemoryAllocateChain &operator=(const MemoryAllocateChain &) = delete;

    /* The chain of `info`, with `kind`'s Win32 export said as the host's fd
     * type and a Win32 import said as `fd`. not_supported for a node this does
     * not carry and for an export of any other handle type, malformed for the
     * same node twice, invalid_parameter for an import with no fd to name, and
     * too_large for a chain past `max_nodes`. Unchanged unless it answers
     * ok. */
    [[nodiscard]] Status take(const VkMemoryAllocateInfo &info, const Win32HandleKind &kind, int fd);
    /* The allocation the host is sent, valid while this is. */
    const VkMemoryAllocateInfo *forwarded() const;
    bool                        exports() const;
    bool                        imports() const;

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
    VkImportMemoryFdInfoKHR       import_;
    VkMemoryDedicatedAllocateInfo dedicated_;
    VkMemoryAllocateFlagsInfo     flags_;
    VkBaseOutStructure           *last_;
    bool                          exports_;
    bool                          imports_;
};

}  // namespace av::icd

#endif  // AV_ICD_MEMORY_ALLOCATE_CHAIN_HPP
