#include "proto/vulkan/fence_wake.hpp"

#include "core/trace/function_scope.hpp"

namespace av::vkproto {

FenceWake::FenceWake() : arm_timeout_(~uint64_t{0} - 1u)
{
    AV_SCOPE("FenceWake::FenceWake");
}

uint64_t FenceWake::arm_timeout() const
{
    AV_SCOPE("FenceWake::arm_timeout");
    return arm_timeout_;
}

bool FenceWake::arms(uint64_t timeout) const
{
    AV_SCOPE("FenceWake::arms");
    return timeout == arm_timeout_;
}

}
