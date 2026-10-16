#include "icd/vulkan/fence_wait_probe.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"

namespace av::icd {

FenceWaitProbe::FenceWaitProbe(ClientTransport &wire, VkDevice device, uint32_t count, const VkFence *fences,
                               VkBool32 wait_all)
    : wire_(wire), device_(device), count_(count), fences_(fences), wait_all_(wait_all), result_(VK_TIMEOUT)
{
    AV_SCOPE("FenceWaitProbe::FenceWaitProbe");
}

bool FenceWaitProbe::ask(uint64_t slice)
{
    AV_SCOPE("FenceWaitProbe::ask");
    result_ = vkproto::av_vkWaitForFences(wire_, device_, count_, fences_, wait_all_, slice);
    return result_ != VK_TIMEOUT;
}

void FenceWaitProbe::pause(uint64_t)
{
    AV_SCOPE("FenceWaitProbe::pause");
}

VkResult FenceWaitProbe::result() const
{
    AV_SCOPE("FenceWaitProbe::result");
    return result_;
}

}  // namespace av::icd
