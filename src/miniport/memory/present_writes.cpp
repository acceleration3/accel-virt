#include "miniport/memory/present_writes.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

PresentWrites::PresentWrites(Platform &platform) : platform_(platform), sequence_(0u), ticket_(0u)
{
    AV_SCOPE("PresentWrites::PresentWrites");
}

void PresentWrites::opening()
{
    AV_SCOPE("PresentWrites::opening");
    platform_.require_irql_at_most(Irql::dispatch, "PresentWrites::opening");
    platform_.store_atomic(sequence_, platform_.load_atomic(sequence_) + 1u);
}

void PresentWrites::written(uint64_t ticket)
{
    AV_SCOPE("PresentWrites::written");
    platform_.require_irql_at_most(Irql::dispatch, "PresentWrites::written");
    if (ticket != 0u) platform_.store_atomic(ticket_, ticket);
    platform_.store_atomic(sequence_, platform_.load_atomic(sequence_) + 1u);
}

uint64_t PresentWrites::sequence()
{
    AV_SCOPE("PresentWrites::sequence");
    platform_.require_irql_at_most(Irql::dispatch, "PresentWrites::sequence");
    return platform_.load_atomic(sequence_);
}

bool PresentWrites::in_progress(uint64_t sequence) const
{
    AV_SCOPE("PresentWrites::in_progress");
    platform_.require_irql_at_most(Irql::dispatch, "PresentWrites::in_progress");
    return (sequence & 1u) != 0u;
}

uint64_t PresentWrites::ticket()
{
    AV_SCOPE("PresentWrites::ticket");
    platform_.require_irql_at_most(Irql::dispatch, "PresentWrites::ticket");
    return platform_.load_atomic(ticket_);
}

bool PresentWrites::invariants_hold()
{
    AV_SCOPE("PresentWrites::invariants_hold");
    return platform_.load_atomic(ticket_) == 0u || platform_.load_atomic(sequence_) != 0u;
}

}
