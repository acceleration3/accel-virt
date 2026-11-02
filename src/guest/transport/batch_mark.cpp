#include "guest/transport/batch_mark.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"

namespace av {

BatchMark::BatchMark() : device_(0u), context_(0u), progress_fence_(0u), fence_(0u)
{
    AV_SCOPE("BatchMark::BatchMark");
}

BatchMark::BatchMark(uint32_t device, uint32_t context, uint32_t progress_fence, uint64_t fence)
    : device_(device), context_(context), progress_fence_(progress_fence), fence_(fence)
{
    AV_SCOPE("BatchMark::BatchMark");
}

bool BatchMark::waitable() const
{
    AV_SCOPE("BatchMark::waitable");
    return device_ != 0u && context_ != 0u && fence_ != 0u;
}

uint64_t BatchMark::fence() const
{
    AV_SCOPE("BatchMark::fence");
    return fence_;
}

Status BatchMark::wait(Kmt &kmt) const
{
    AV_SCOPE("BatchMark::wait");
    if (!waitable()) return Status::invalid_state;
    return progress_fence_ != 0u ? kmt.wait_for_hw_queue(device_, progress_fence_, fence_)
                                 : kmt.wait_for_context(device_, context_);
}

}
