#include "core/platform/wait_mutex.hpp"

namespace av {

WaitMutex::WaitMutex(Platform &platform, LockRank rank) : platform_(platform), storage_{}, rank_(rank)
{
    platform_.require_irql_at_most(Irql::dispatch, "WaitMutex::WaitMutex");
    platform_.initialize_wait_mutex(storage_);
}

void WaitMutex::acquire()
{
    platform_.require_irql_at_most(Irql::passive, "WaitMutex::acquire");
    platform_.acquire_wait_mutex(storage_, rank_);
}

void WaitMutex::release()
{
    platform_.require_irql_at_most(Irql::passive, "WaitMutex::release");
    platform_.release_wait_mutex(storage_, rank_);
}

}  // namespace av
