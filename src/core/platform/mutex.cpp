#include "core/platform/mutex.hpp"

namespace av {

Mutex::Mutex(Platform &platform, LockRank rank)
    : platform_(platform), storage_{}, rank_(rank)
{
    platform_.require_irql_at_most(Irql::dispatch, "Mutex::Mutex");
    platform_.initialize_mutex(storage_);
}

void Mutex::acquire()
{
    platform_.require_irql_at_most(Irql::apc, "Mutex::acquire");
    platform_.acquire_mutex(storage_, rank_);
}

void Mutex::release()
{
    platform_.require_irql_at_most(Irql::apc, "Mutex::release");
    platform_.release_mutex(storage_, rank_);
}

}  // namespace av
