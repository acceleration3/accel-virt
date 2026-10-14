#include "core/platform/wake_event.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

WakeEvent::WakeEvent(Platform &platform) : platform_(platform), storage_{}
{
    AV_SCOPE("WakeEvent::WakeEvent");
    platform_.require_irql_at_most(Irql::dispatch, "WakeEvent::WakeEvent");
    platform_.initialize_wake_event(storage_);
}

void WakeEvent::set()
{
    AV_SCOPE("WakeEvent::set");
    platform_.require_irql_at_most(Irql::dispatch, "WakeEvent::set");
    platform_.set_wake_event(storage_);
}

bool WakeEvent::wait(uint64_t ticks)
{
    AV_SCOPE("WakeEvent::wait");
    platform_.require_irql_at_most(Irql::passive, "WakeEvent::wait");
    return platform_.wait_wake_event(storage_, ticks);
}

}  // namespace av
