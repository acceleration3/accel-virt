#include "core/platform/wait_mutex.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

WaitMutex::WaitMutex(Platform &platform, LockRank rank) : platform_(platform), storage_{}, rank_(rank), marks_(this)
{
    AV_SCOPE("WaitMutex::WaitMutex");
    platform_.require_irql_at_most(Irql::dispatch, "WaitMutex::WaitMutex");
    platform_.initialize_wait_mutex(storage_);
}

void WaitMutex::acquire()
{
    AV_SCOPE("WaitMutex::acquire");
    platform_.require_irql_at_most(Irql::passive, "WaitMutex::acquire");
    marks_.waiting();
    platform_.acquire_wait_mutex(storage_, rank_);
    marks_.acquired();
}

void WaitMutex::release()
{
    AV_SCOPE("WaitMutex::release");
    platform_.require_irql_at_most(Irql::passive, "WaitMutex::release");
    marks_.released();
    platform_.release_wait_mutex(storage_, rank_);
}

}  // namespace av
