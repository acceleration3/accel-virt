/* vkWaitSemaphores asked of the host a slice at a time (HostCompletionWait):
 * the host blocks for the slice itself, so nothing pauses between asks, and
 * any answer but VK_TIMEOUT ends the wait. */
#ifndef AV_ICD_SEMAPHORE_WAIT_PROBE_HPP
#define AV_ICD_SEMAPHORE_WAIT_PROBE_HPP

#include "icd/common/completion_probe.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class ClientTransport;
}  // namespace av

namespace av::icd {

class SemaphoreWaitProbe final : public CompletionProbe {
public:
    SemaphoreWaitProbe(ClientTransport &wire, VkDevice device, const VkSemaphoreWaitInfo *info);

    [[nodiscard]] bool ask(uint64_t slice) override;
    void               pause(uint64_t slice) override;
    /* The host's last answer: VK_TIMEOUT until one ends the wait. */
    VkResult           result() const;

private:
    ClientTransport           &wire_;
    VkDevice                   device_;
    const VkSemaphoreWaitInfo *info_;
    VkResult                   result_;
};

}  // namespace av::icd

#endif  // AV_ICD_SEMAPHORE_WAIT_PROBE_HPP
