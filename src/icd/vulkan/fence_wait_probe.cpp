#include "icd/vulkan/fence_wait_probe.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"
#include "proto/vulkan/fence_wake.hpp"

namespace av::icd {

FenceWaitProbe::FenceWaitProbe(ClientTransport &wire, vkproto::HandleTranslation &handles, Kmt &kmt, FenceWakes &wakes, bool driven,
                               VkDevice device, uint32_t count, const VkFence *fences, VkBool32 wait_all)
    : wire_(wire), handles_(handles), polls_(kmt), wakes_(wakes), driven_(driven), armed_(false), seen_(0u), device_(device),
      count_(count), fences_(fences), wait_all_(wait_all), result_(VK_TIMEOUT)
{
    AV_SCOPE("FenceWaitProbe::FenceWaitProbe");
}

bool FenceWaitProbe::ask(uint64_t)
{
    AV_SCOPE("FenceWaitProbe::ask");
    seen_ = driven_ ? wakes_.woken() : 0u;
    const bool arming = driven_ && !armed_;
    result_ = vkproto::av_vkWaitForFences(wire_, handles_, device_, count_, fences_, wait_all_,
                                          arming ? vkproto::FenceWake::arm_timeout : 0u);
    if (result_ == VK_NOT_READY) {
        driven_ = false;
        result_ = VK_TIMEOUT;
    } else if (result_ == VK_TIMEOUT && arming) {
        armed_ = true;
    }
    return result_ != VK_TIMEOUT;
}

void FenceWaitProbe::pause(uint64_t slice)
{
    AV_SCOPE("FenceWaitProbe::pause");
    if (polls_.spinning()) return;
    if (driven_ && armed_) wakes_.await(seen_, slice);
    else polls_.rest();
}

VkResult FenceWaitProbe::result() const
{
    AV_SCOPE("FenceWaitProbe::result");
    return result_;
}

}
