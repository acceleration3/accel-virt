#include "core/platform/mutex.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

Mutex::Mutex(Platform &platform, LockRank rank)
    : platform_(platform), storage_{}, rank_(rank), marks_(this)
{
    AV_SCOPE("Mutex::Mutex");
    platform_.require_irql_at_most(Irql::dispatch, "Mutex::Mutex");
    platform_.initialize_mutex(storage_);
}

void Mutex::acquire()
{
    AV_SCOPE("Mutex::acquire");
    platform_.require_irql_at_most(Irql::apc, "Mutex::acquire");
    marks_.waiting();
    platform_.acquire_mutex(storage_, rank_);
    marks_.acquired();
}

bool Mutex::try_acquire()
{
    AV_SCOPE("Mutex::try_acquire");
    platform_.require_irql_at_most(Irql::apc, "Mutex::try_acquire");
    if (!platform_.try_acquire_mutex(storage_, rank_)) return false;
    marks_.acquired();
    return true;
}

void Mutex::release()
{
    AV_SCOPE("Mutex::release");
    platform_.require_irql_at_most(Irql::apc, "Mutex::release");
    marks_.released();
    platform_.release_mutex(storage_, rank_);
}

}  // namespace av
