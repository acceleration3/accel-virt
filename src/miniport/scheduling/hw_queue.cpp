#include "miniport/scheduling/hw_queue.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

HwQueue::HwQueue(Platform &platform, Context &context, uint64_t *progress)
    : platform_(platform), context_(context), progress_(progress)
{
    AV_SCOPE("HwQueue::HwQueue");
}

uint64_t *HwQueue::progress() const
{
    AV_SCOPE("HwQueue::progress");
    platform_.require_irql_at_most(Irql::dispatch, "HwQueue::progress");
    return progress_;
}

Context &HwQueue::context() const
{
    AV_SCOPE("HwQueue::context");
    /* A submission to a hardware queue arrives at DISPATCH and reaches its
     * context from the handle alone (feature 049): the reference is fixed at
     * construction, so reading it blocks on nothing. */
    platform_.require_irql_at_most(Irql::dispatch, "HwQueue::context");
    return context_;
}

}  // namespace av
