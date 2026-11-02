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
    static constexpr uint32_t carried_kinds = 4u;

    MemoryAllocateChain();
    MemoryAllocateChain(const MemoryAllocateChain &) = delete;
    MemoryAllocateChain &operator=(const MemoryAllocateChain &) = delete;

    [[nodiscard]] Status take(const VkMemoryAllocateInfo &info, const Win32HandleKind &kind, VkFlags share_type);
    const VkMemoryAllocateInfo *forwarded() const;
    bool                        exports() const;

    bool invariants_hold() const;

private:
    void   link(void *node);
    void   clear();
    Status refusal(Status why);

    VkMemoryAllocateInfo          info_;
    VkExportMemoryAllocateInfo    export_;
    VkMemoryDedicatedAllocateInfo dedicated_;
    VkMemoryAllocateFlagsInfo     flags_;
    VkBaseOutStructure           *last_;
    bool                          exports_;

    friend struct MemoryAllocateChainProbe;
};

}

#endif
