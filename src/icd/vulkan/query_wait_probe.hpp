/* vkGetQueryPoolResults with VK_QUERY_RESULT_WAIT_BIT, which the host would
 * otherwise block in, unbounded, holding the context's worker past TdrDelay.
 * The host is asked without the bit - it answers at once, VK_NOT_READY while
 * a query is unavailable - and asked again, pausing as a poll does
 * (PollPause), until every query is available (HostCompletionWait). */
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
}  // namespace av

namespace av::icd {

class QueryWaitProbe final : public CompletionProbe {
public:
    /* The application's call, `flags` without VK_QUERY_RESULT_WAIT_BIT. */
    QueryWaitProbe(ClientTransport &wire, Kmt &kmt, VkDevice device, VkQueryPool pool, uint32_t first,
                   uint32_t count, size_t size, void *data, VkDeviceSize stride, VkQueryResultFlags flags);

    [[nodiscard]] bool ask(uint64_t slice) override;
    void               pause(uint64_t slice) override;
    /* The host's last answer: VK_NOT_READY until every query is available. */
    VkResult           result() const;

private:
    ClientTransport   &wire_;
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

}  // namespace av::icd

#endif  // AV_ICD_QUERY_WAIT_PROBE_HPP
