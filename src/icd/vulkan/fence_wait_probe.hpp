#ifndef AV_ICD_FENCE_WAIT_PROBE_HPP
#define AV_ICD_FENCE_WAIT_PROBE_HPP

#include "icd/common/completion_probe.hpp"
#include "icd/common/poll_pause.hpp"
#include "icd/vulkan/fence_wakes.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class ClientTransport;
class Kmt;
}

namespace av::vkproto {
class HandleTranslation;
}

namespace av::icd {

class FenceWaitProbe final : public CompletionProbe {
public:
    FenceWaitProbe(ClientTransport &wire, vkproto::HandleTranslation &handles, Kmt &kmt, FenceWakes &wakes, bool driven,
                   VkDevice device, uint32_t count, const VkFence *fences, VkBool32 wait_all);

    [[nodiscard]] bool ask(uint64_t slice) override;
    void               pause(uint64_t slice) override;
    VkResult           result() const;

private:
    ClientTransport &wire_;
    vkproto::HandleTranslation &handles_;
    PollPause        polls_;
    FenceWakes      &wakes_;
    bool             driven_;
    bool             armed_;
    uint64_t         seen_;
    VkDevice         device_;
    uint32_t         count_;
    const VkFence   *fences_;
    VkBool32         wait_all_;
    VkResult         result_;
};

}

#endif
