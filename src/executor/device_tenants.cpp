#include "executor/device_tenants.hpp"

#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/wake_signal.hpp"

namespace av {

DeviceTenants::DeviceTenants(BlockSource &blocks)
    : lock_(), left_(lock_), tenants_(blocks, Pool::paged), lost_(false), clock_()
{
    AV_SCOPE("DeviceTenants::DeviceTenants");
}

Status DeviceTenants::join(WakeSignal &wake)
{
    AV_SCOPE("DeviceTenants::join");
    const ThreadLockHold held(lock_);
    Status               joined = lost_ ? Status::invalid_state : Status::ok;
    for (uint32_t index = 0u; joined == Status::ok && index < tenants_.size(); index++)
        if (tenants_.at(index) == &wake) joined = Status::conflict;
    if (joined == Status::ok) joined = tenants_.append(&wake);
    return joined;
}

void DeviceTenants::leave(WakeSignal &wake)
{
    AV_SCOPE("DeviceTenants::leave");
    const ThreadLockHold held(lock_);
    for (uint32_t index = 0u; index < tenants_.size(); index++) {
        if (tenants_.at(index) != &wake) continue;
        tenants_.remove_swap(index);
        break;
    }
    if (tenants_.size() == 0u) left_.broadcast();
}

bool DeviceTenants::lose()
{
    AV_SCOPE("DeviceTenants::lose");
    const ThreadLockHold held(lock_);
    const bool           first = !lost_;
    __atomic_store_n(&lost_, true, __ATOMIC_RELEASE);
    if (first)
        for (WakeSignal *tenant : tenants_) static_cast<void>(tenant->raise());
    return first;
}

bool DeviceTenants::lost() const
{
    AV_SCOPE("DeviceTenants::lost");
    return __atomic_load_n(&lost_, __ATOMIC_ACQUIRE);
}

uint32_t DeviceTenants::count() const
{
    AV_SCOPE("DeviceTenants::count");
    const ThreadLockHold held(lock_);
    return tenants_.size();
}

bool DeviceTenants::vacate(uint32_t wait_ms)
{
    AV_SCOPE("DeviceTenants::vacate");
    constexpr uint64_t nanoseconds_per_ms = 1000000u;
    const timespec     deadline = clock_.ahead(static_cast<uint64_t>(wait_ms) * nanoseconds_per_ms);
    const ThreadLockHold held(lock_);
    bool                 in_time = true;
    while (tenants_.size() != 0u && in_time) in_time = left_.wait_until(deadline);
    return tenants_.size() == 0u;
}

Status DeviceTenants::renew()
{
    AV_SCOPE("DeviceTenants::renew");
    const ThreadLockHold held(lock_);
    const Status         renewed = !lost_ || tenants_.size() != 0u ? Status::invalid_state : Status::ok;
    if (renewed == Status::ok) __atomic_store_n(&lost_, false, __ATOMIC_RELEASE);
    return renewed;
}

bool DeviceTenants::invariants_hold() const
{
    AV_SCOPE("DeviceTenants::invariants_hold");
    const ThreadLockHold held(lock_);
    bool                 holds = true;
    for (uint32_t index = 0u; holds && index < tenants_.size(); index++) {
        holds = tenants_.at(index) != nullptr;
        for (uint32_t other = index + 1u; holds && other < tenants_.size(); other++)
            holds = tenants_.at(index) != tenants_.at(other);
    }
    return holds;
}

}
