#include "executor/shim/gated_queue.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/context_gate.hpp"

namespace av {

GatedQueue::GatedQueue(BorrowedQueue &inner, ContextGate &gate) : inner_(inner), gate_(gate)
{
    AV_SCOPE("GatedQueue::GatedQueue");
}

VkQueue GatedQueue::queue() const
{
    AV_SCOPE("GatedQueue::queue");
    return inner_.queue();
}

uint32_t GatedQueue::family() const
{
    AV_SCOPE("GatedQueue::family");
    return inner_.family();
}

void GatedQueue::flush()
{
    AV_SCOPE("GatedQueue::flush");
    if (gate_.inside()) {
        inner_.flush();
        return;
    }
    gate_.enter();
    inner_.flush();
    gate_.leave();
}

void GatedQueue::lock()
{
    AV_SCOPE("GatedQueue::lock");
    inner_.lock();
}

void GatedQueue::unlock()
{
    AV_SCOPE("GatedQueue::unlock");
    inner_.unlock();
}

}
