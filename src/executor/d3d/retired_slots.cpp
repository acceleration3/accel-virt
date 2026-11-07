#include "executor/d3d/retired_slots.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/d3d/idle_queue.hpp"

namespace av {

RetiredSlots::RetiredSlots() : drains_{}
{
    AV_SCOPE("RetiredSlots::RetiredSlots");
}

bool RetiredSlots::retire(uint32_t slot, IdleQueue &queue)
{
    AV_SCOPE("RetiredSlots::retire");
    if (slot >= capacity) return false;
    return drains_[slot].mark(queue, 1u);
}

bool RetiredSlots::done(uint32_t slot, const IdleQueue &queue) const
{
    AV_SCOPE("RetiredSlots::done");
    return slot < capacity && drains_[slot].reached(queue, 0u);
}

void RetiredSlots::freed(uint32_t slot)
{
    AV_SCOPE("RetiredSlots::freed");
    if (slot < capacity) drains_[slot].forget();
}

bool RetiredSlots::retired(uint32_t slot) const
{
    AV_SCOPE("RetiredSlots::retired");
    return slot < capacity && drains_[slot].marked();
}

bool RetiredSlots::any() const
{
    AV_SCOPE("RetiredSlots::any");
    for (const QueueDrain &drain : drains_)
        if (drain.marked()) return true;
    return false;
}

}
