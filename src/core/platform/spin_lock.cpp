#include "core/platform/spin_lock.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

SpinLock::SpinLock(Platform &platform, LockRank rank)
    : platform_(platform), word_(0u), rank_(rank), marks_(this)
{
    AV_SCOPE("SpinLock::SpinLock");
}

Irql SpinLock::acquire()
{
    AV_SCOPE("SpinLock::acquire");
    platform_.require_irql_at_most(Irql::dispatch, "SpinLock::acquire");
    marks_.waiting();
    const Irql previous = platform_.acquire_spin_lock(word_, rank_);
    marks_.acquired();
    return previous;
}

bool SpinLock::try_acquire(Irql &previous)
{
    AV_SCOPE("SpinLock::try_acquire");
    platform_.require_irql_at_most(Irql::dispatch, "SpinLock::try_acquire");
    if (!platform_.try_acquire_spin_lock(word_, rank_, previous)) return false;
    marks_.acquired();
    return true;
}

void SpinLock::release(Irql previous)
{
    AV_SCOPE("SpinLock::release");
    platform_.require_irql_at_most(Irql::dispatch, "SpinLock::release");
    marks_.released();
    platform_.release_spin_lock(word_, rank_, previous);
}

}
