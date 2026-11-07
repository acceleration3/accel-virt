#include "executor/d3d/queue_drain.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/d3d/idle_queue.hpp"

namespace av {

QueueDrain::QueueDrain() : ticket_(0u), work_(0u), marked_(false)
{
    AV_SCOPE("QueueDrain::QueueDrain");
}

bool QueueDrain::mark(IdleQueue &queue, uint64_t work)
{
    AV_SCOPE("QueueDrain::mark");
    if (marked_ && work_ == work) return true;
    uint64_t ticket = 0u;
    if (!queue.mark_idle(ticket)) return false;
    ticket_ = ticket;
    work_ = work;
    marked_ = true;
    return true;
}

bool QueueDrain::reached(const IdleQueue &queue, uint64_t nanoseconds) const
{
    AV_SCOPE("QueueDrain::reached");
    return marked_ && queue.reached(ticket_, nanoseconds);
}

void QueueDrain::forget()
{
    AV_SCOPE("QueueDrain::forget");
    marked_ = false;
    ticket_ = 0u;
    work_ = 0u;
}

bool QueueDrain::marked() const
{
    AV_SCOPE("QueueDrain::marked");
    return marked_;
}

}
