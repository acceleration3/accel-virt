#include "icd/vulkan/query_wait_probe.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"

namespace av::icd {

QueryWaitProbe::QueryWaitProbe(ClientTransport &wire, Kmt &kmt, VkDevice device, VkQueryPool pool, uint32_t first,
                               uint32_t count, size_t size, void *data, VkDeviceSize stride,
                               VkQueryResultFlags flags)
    : wire_(wire), polls_(kmt), device_(device), pool_(pool), first_(first), count_(count), size_(size), data_(data),
      stride_(stride), flags_(flags & ~VkQueryResultFlags{VK_QUERY_RESULT_WAIT_BIT}), result_(VK_NOT_READY)
{
    AV_SCOPE("QueryWaitProbe::QueryWaitProbe");
}

bool QueryWaitProbe::ask(uint64_t)
{
    AV_SCOPE("QueryWaitProbe::ask");
    result_ = vkproto::av_vkGetQueryPoolResults(wire_, device_, pool_, first_, count_, size_, data_, stride_, flags_);
    return result_ != VK_NOT_READY;
}

void QueryWaitProbe::pause(uint64_t)
{
    AV_SCOPE("QueryWaitProbe::pause");
    polls_.between();
}

VkResult QueryWaitProbe::result() const
{
    AV_SCOPE("QueryWaitProbe::result");
    return result_;
}

}  // namespace av::icd
