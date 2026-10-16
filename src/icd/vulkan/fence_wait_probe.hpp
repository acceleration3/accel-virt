/* vkWaitForFences asked of the host a slice at a time (HostCompletionWait):
 * the host blocks for the slice itself, so nothing pauses between asks, and
 * any answer but VK_TIMEOUT ends the wait. */
#ifndef AV_ICD_FENCE_WAIT_PROBE_HPP
#define AV_ICD_FENCE_WAIT_PROBE_HPP

#include "icd/common/completion_probe.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class ClientTransport;
}  // namespace av

namespace av::vkproto {
class HandleTranslation;
}  // namespace av::vkproto

namespace av::icd {

class FenceWaitProbe final : public CompletionProbe {
public:
    FenceWaitProbe(ClientTransport &wire, vkproto::HandleTranslation &handles, VkDevice device, uint32_t count, const VkFence *fences, VkBool32 wait_all);

    [[nodiscard]] bool ask(uint64_t slice) override;
    void               pause(uint64_t slice) override;
    /* The host's last answer: VK_TIMEOUT until one ends the wait. */
    VkResult           result() const;

private:
    ClientTransport &wire_;
    vkproto::HandleTranslation &handles_;
    VkDevice         device_;
    uint32_t         count_;
    const VkFence   *fences_;
    VkBool32         wait_all_;
    VkResult         result_;
};

}  // namespace av::icd

#endif  // AV_ICD_FENCE_WAIT_PROBE_HPP
