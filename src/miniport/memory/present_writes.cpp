#include "miniport/memory/present_writes.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

PresentWrites::PresentWrites(Platform &platform)
    : platform_(platform), sequence_(0u), writers_(0u), ticket_(0u), epoch_(0u)
{
    AV_SCOPE("PresentWrites::PresentWrites");
}

/* The writer is counted before the sequence moves: a reader that sees the new
 * sequence sees the writer too. */
void PresentWrites::opening()
{
    AV_SCOPE("PresentWrites::opening");
    platform_.require_irql_at_most(Irql::dispatch, "PresentWrites::opening");
    static_cast<void>(platform_.add_atomic(writers_, 1u));
    static_cast<void>(platform_.add_atomic(sequence_, 1u));
}

/* Adding the all-ones is subtracting one. */
void PresentWrites::written(uint64_t ticket, uint32_t epoch)
{
    AV_SCOPE("PresentWrites::written");
    platform_.require_irql_at_most(Irql::dispatch, "PresentWrites::written");
    if (ticket != 0u) {
        platform_.store_atomic(epoch_, epoch);
        platform_.store_atomic(ticket_, ticket);
    }
    static_cast<void>(platform_.add_atomic(sequence_, 1u));
    static_cast<void>(platform_.add_atomic(writers_, ~uint64_t{0}));
}

uint64_t PresentWrites::sequence()
{
    AV_SCOPE("PresentWrites::sequence");
    platform_.require_irql_at_most(Irql::dispatch, "PresentWrites::sequence");
    return platform_.load_atomic(sequence_);
}

bool PresentWrites::in_progress(uint64_t sequence)
{
    AV_SCOPE("PresentWrites::in_progress");
    platform_.require_irql_at_most(Irql::dispatch, "PresentWrites::in_progress");
    return (sequence & 1u) != 0u || platform_.load_atomic(writers_) != 0u;
}

uint64_t PresentWrites::ticket()
{
    AV_SCOPE("PresentWrites::ticket");
    platform_.require_irql_at_most(Irql::dispatch, "PresentWrites::ticket");
    return platform_.load_atomic(ticket_);
}

uint32_t PresentWrites::epoch()
{
    AV_SCOPE("PresentWrites::epoch");
    platform_.require_irql_at_most(Irql::dispatch, "PresentWrites::epoch");
    return static_cast<uint32_t>(platform_.load_atomic(epoch_));
}

/* A ticket is only ever stored after a present opened the pages, and a
 * writer open is one more opening than the sequence has counted closed. */
bool PresentWrites::invariants_hold()
{
    AV_SCOPE("PresentWrites::invariants_hold");
    const uint64_t sequence = platform_.load_atomic(sequence_);
    const bool     ticketed_after_opening = platform_.load_atomic(ticket_) == 0u || sequence != 0u;
    return ticketed_after_opening && platform_.load_atomic(writers_) <= sequence;
}

}  // namespace av
