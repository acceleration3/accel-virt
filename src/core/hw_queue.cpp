#include "hw_queue.hpp"

namespace av {

HwQueue::HwQueue(Platform &platform, Context &context) : platform_(platform), context_(context) {}

Context &HwQueue::context() const
{
    platform_.require_irql_at_most(Irql::apc, "HwQueue::context");
    return context_;
}

}  // namespace av
