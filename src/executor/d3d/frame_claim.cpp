#include "executor/d3d/frame_claim.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

FrameClaim::FrameClaim() : out_(0u)
{
    AV_SCOPE("FrameClaim::FrameClaim");
}

bool FrameClaim::claim()
{
    AV_SCOPE("FrameClaim::claim");
    uint32_t free = 0u;
    return __atomic_compare_exchange_n(&out_, &free, 1u, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}

void FrameClaim::give_back()
{
    AV_SCOPE("FrameClaim::give_back");
    __atomic_store_n(&out_, 0u, __ATOMIC_RELEASE);
}

bool FrameClaim::out() const
{
    AV_SCOPE("FrameClaim::out");
    return __atomic_load_n(&out_, __ATOMIC_ACQUIRE) != 0u;
}

}
