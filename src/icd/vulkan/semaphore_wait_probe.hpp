#ifndef AV_ICD_SEMAPHORE_WAIT_PROBE_HPP
#define AV_ICD_SEMAPHORE_WAIT_PROBE_HPP

#include "icd/common/completion_probe.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class ClientTransport;
}

namespace av::vkproto {
class HandleTranslation;
}

namespace av::icd {

class SemaphoreWaitProbe final : public CompletionProbe {
public:
    SemaphoreWaitProbe(ClientTransport &wire, vkproto::HandleTranslation &handles, VkDevice device, const VkSemaphoreWaitInfo *info);

    [[nodiscard]] bool ask(uint64_t slice) override;
    void               pause(uint64_t slice) override;
    VkResult           result() const;

private:
    ClientTransport           &wire_;
    vkproto::HandleTranslation &handles_;
    VkDevice                   device_;
    const VkSemaphoreWaitInfo *info_;
    VkResult                   result_;
};

}

#endif
