#include "core/scheduling/hw_queue.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

HwQueue::HwQueue(Platform &platform, Context &context) : platform_(platform), context_(context)
{
    AV_SCOPE("HwQueue::HwQueue");
}

Context &HwQueue::context() const
{
    AV_SCOPE("HwQueue::context");
    platform_.require_irql_at_most(Irql::apc, "HwQueue::context");
    return context_;
}

}  // namespace av
