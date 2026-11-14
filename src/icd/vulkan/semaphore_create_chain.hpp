#ifndef AV_ICD_SEMAPHORE_CREATE_CHAIN_HPP
#define AV_ICD_SEMAPHORE_CREATE_CHAIN_HPP

#include "core/support/status.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av::icd {

class Win32HandleKind;
struct SemaphoreCreateChainProbe;

class SemaphoreCreateChain {
public:
    static constexpr uint32_t carried_kinds = 3u;

    SemaphoreCreateChain();
    SemaphoreCreateChain(const SemaphoreCreateChain &) = delete;
    SemaphoreCreateChain &operator=(const SemaphoreCreateChain &) = delete;

    [[nodiscard]] Status take(const VkSemaphoreCreateInfo &info, const Win32HandleKind &kind, VkFlags share_type);
    const VkSemaphoreCreateInfo *forwarded() const;

    bool invariants_hold() const;

private:
    void   link(void *node);
    void   clear();
    Status refusal(Status why);

    VkSemaphoreCreateInfo       info_;
    VkExportSemaphoreCreateInfo export_;
    VkSemaphoreTypeCreateInfo   typed_;
    VkBaseOutStructure         *last_;

    friend struct SemaphoreCreateChainProbe;
};

}

#endif
