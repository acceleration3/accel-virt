#ifndef AV_ICD_QUERY_WAIT_PROBE_HPP
#define AV_ICD_QUERY_WAIT_PROBE_HPP

#include "icd/common/completion_probe.hpp"
#include "icd/common/poll_pause.hpp"

#include <stddef.h>
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

class QueryWaitProbe final : public CompletionProbe {
public:
    QueryWaitProbe(ClientTransport &wire, vkproto::HandleTranslation &handles, Kmt &kmt, VkDevice device, VkQueryPool pool, uint32_t first,
                   uint32_t count, size_t size, void *data, VkDeviceSize stride, VkQueryResultFlags flags);

    [[nodiscard]] bool ask(uint64_t slice) override;
    void               pause(uint64_t slice) override;
    VkResult           result() const;

private:
    ClientTransport   &wire_;
    vkproto::HandleTranslation &handles_;
    PollPause          polls_;
    VkDevice           device_;
    VkQueryPool        pool_;
    uint32_t           first_;
    uint32_t           count_;
    size_t             size_;
    void              *data_;
    VkDeviceSize       stride_;
    VkQueryResultFlags flags_;
    VkResult           result_;
};

}

#endif
