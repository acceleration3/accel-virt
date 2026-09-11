#include "core/platform/spin_lock.hpp"

namespace av {

SpinLock::SpinLock(Platform &platform, LockRank rank)
    : platform_(platform), word_(0u), rank_(rank) {}

Irql SpinLock::acquire()
{
    platform_.require_irql_at_most(Irql::dispatch, "SpinLock::acquire");
    return platform_.acquire_spin_lock(word_, rank_);
}

void SpinLock::release(Irql previous)
{
    platform_.require_irql_at_most(Irql::dispatch, "SpinLock::release");
    platform_.release_spin_lock(word_, rank_, previous);
}

}  // namespace av
