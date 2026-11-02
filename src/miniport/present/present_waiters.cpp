#include "miniport/present/present_waiters.hpp"

#include "core/platform/spin_lock_guard.hpp"
#include "core/platform/spin_lock_try_guard.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "miniport/memory/present_writes.hpp"
#include "miniport/present/present_page_view.hpp"
#include "proto/pages/present_progress.hpp"

namespace av {

PresentWaiters::PresentWaiters(Platform &platform)
    : platform_(platform), lock_(platform, LockRank::present_waiters),
      events_{WakeEvent(platform), WakeEvent(platform), WakeEvent(platform), WakeEvent(platform)}, taken_{},
      waits_(0u), timeouts_(0u)
{
    AV_SCOPE("PresentWaiters::PresentWaiters");
}

Status PresentWaiters::wait_for(PresentWrites &writes, const PresentPageView *progress, uint64_t ticks,
                                uint64_t &sequence)
{
    AV_SCOPE("PresentWaiters::wait_for");
    platform_.require_irql_at_most(Irql::passive, "PresentWaiters::wait_for");
    if (settled(writes, progress, sequence)) return Status::ok;
    const uint64_t begun = platform_.interrupt_time();
    const uint64_t deadline = ticks > ~uint64_t{0} - begun ? ~uint64_t{0} : begun + ticks;
    const uint32_t slot = take();
    if (slot == slots) return Status::no_space;
    bool timed_out = false;
    for (;;) {
        if (settled(writes, progress, sequence)) break;
        const uint64_t now = platform_.interrupt_time();
        timed_out = now >= deadline;
        if (timed_out) break;
        static_cast<void>(events_[slot].wait(deadline - now));
    }
    give(slot, timed_out);
    return timed_out ? Status::not_found : Status::ok;
}

bool PresentWaiters::settled(PresentWrites &writes, const PresentPageView *progress, uint64_t &sequence)
{
    AV_SCOPE("PresentWaiters::settled");
    const uint64_t first = writes.sequence();
    if (writes.in_progress(first)) return false;
    const uint64_t ticket = writes.ticket();
    if (writes.sequence() != first) return false;
    PresentProgress engine;
    if (progress != nullptr && ticket != 0u) {
        const uint64_t completed = progress->read(engine) == Status::ok ? engine.completed() : 0u;
        if (completed < ticket) return false;
    }
    sequence = first;
    return true;
}

uint32_t PresentWaiters::take()
{
    AV_SCOPE("PresentWaiters::take");
    SpinLockGuard guard(lock_);
    for (uint32_t slot = 0u; slot < slots; slot++) {
        if (taken_[slot]) continue;
        taken_[slot] = true;
        return slot;
    }
    return slots;
}

void PresentWaiters::give(uint32_t slot, bool timed_out)
{
    AV_SCOPE("PresentWaiters::give");
    SpinLockGuard guard(lock_);
    taken_[slot] = false;
    waits_++;
    if (timed_out) timeouts_++;
}

void PresentWaiters::wake()
{
    AV_SCOPE("PresentWaiters::wake");
    platform_.require_irql_at_most(Irql::dispatch, "PresentWaiters::wake");
    SpinLockGuard guard(lock_);
    for (uint32_t slot = 0u; slot < slots; slot++)
        if (taken_[slot]) events_[slot].set();
}

uint64_t PresentWaiters::waits()
{
    AV_SCOPE("PresentWaiters::waits");
    platform_.require_irql_at_most(Irql::dispatch, "PresentWaiters::waits");
    SpinLockGuard guard(lock_);
    return waits_;
}

uint64_t PresentWaiters::timeouts()
{
    AV_SCOPE("PresentWaiters::timeouts");
    platform_.require_irql_at_most(Irql::dispatch, "PresentWaiters::timeouts");
    SpinLockGuard guard(lock_);
    return timeouts_;
}

void PresentWaiters::report(StateWriter &writer)
{
    AV_SCOPE("PresentWaiters::report");
    platform_.require_irql_at_most(Irql::dispatch, "PresentWaiters::report");
    const SpinLockTryGuard guard(lock_);
    writer.open("present_waiters", 0u);
    writer.flag("busy", !guard.held());
    if (guard.held()) {
        uint32_t waiting = 0u;
        for (uint32_t slot = 0u; slot < slots; slot++)
            if (taken_[slot]) waiting++;
        writer.number("waiting", waiting);
        writer.number("waits", waits_);
        writer.number("timeouts", timeouts_);
    }
    writer.close();
}

bool PresentWaiters::invariants_hold()
{
    AV_SCOPE("PresentWaiters::invariants_hold");
    SpinLockGuard guard(lock_);
    return timeouts_ <= waits_;
}

}
