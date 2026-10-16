#include "icd/vulkan/semaphore_wait_probe.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"

namespace av::icd {

SemaphoreWaitProbe::SemaphoreWaitProbe(ClientTransport &wire, VkDevice device, const VkSemaphoreWaitInfo *info)
    : wire_(wire), device_(device), info_(info), result_(VK_TIMEOUT)
{
    AV_SCOPE("SemaphoreWaitProbe::SemaphoreWaitProbe");
}

bool SemaphoreWaitProbe::ask(uint64_t slice)
{
    AV_SCOPE("SemaphoreWaitProbe::ask");
    result_ = vkproto::av_vkWaitSemaphores(wire_, device_, info_, slice);
    return result_ != VK_TIMEOUT;
}

void SemaphoreWaitProbe::pause(uint64_t)
{
    AV_SCOPE("SemaphoreWaitProbe::pause");
}

VkResult SemaphoreWaitProbe::result() const
{
    AV_SCOPE("SemaphoreWaitProbe::result");
    return result_;
}

}  // namespace av::icd
