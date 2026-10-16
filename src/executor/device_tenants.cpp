#include "executor/device_tenants.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/wake_signal.hpp"

#include <time.h>

namespace av {

DeviceTenants::DeviceTenants(BlockSource &blocks)
    : mutex_(), left_(), tenants_(blocks, Pool::paged), lost_(false), clock_()
{
    AV_SCOPE("DeviceTenants::DeviceTenants");
    pthread_mutex_init(&mutex_, nullptr);
    pthread_condattr_t attributes;
    pthread_condattr_init(&attributes);
    pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
    pthread_cond_init(&left_, &attributes);
    pthread_condattr_destroy(&attributes);
}

DeviceTenants::~DeviceTenants()
{
    AV_SCOPE("DeviceTenants::~DeviceTenants");
    pthread_cond_destroy(&left_);
    pthread_mutex_destroy(&mutex_);
}

Status DeviceTenants::join(WakeSignal &wake)
{
    AV_SCOPE("DeviceTenants::join");
    pthread_mutex_lock(&mutex_);
    Status joined = lost_ ? Status::invalid_state : Status::ok;
    for (uint32_t index = 0u; joined == Status::ok && index < tenants_.size(); index++)
        if (tenants_.at(index) == &wake) joined = Status::conflict;
    if (joined == Status::ok) joined = tenants_.append(&wake);
    pthread_mutex_unlock(&mutex_);
    return joined;
}

void DeviceTenants::leave(WakeSignal &wake)
{
    AV_SCOPE("DeviceTenants::leave");
    pthread_mutex_lock(&mutex_);
    for (uint32_t index = 0u; index < tenants_.size(); index++) {
        if (tenants_.at(index) != &wake) continue;
        tenants_.remove_swap(index);
        break;
    }
    if (tenants_.size() == 0u) pthread_cond_broadcast(&left_);
    pthread_mutex_unlock(&mutex_);
}

bool DeviceTenants::lose()
{
    AV_SCOPE("DeviceTenants::lose");
    pthread_mutex_lock(&mutex_);
    const bool first = !lost_;
    __atomic_store_n(&lost_, true, __ATOMIC_RELEASE);
    if (first)
        for (WakeSignal *tenant : tenants_) static_cast<void>(tenant->raise());
    pthread_mutex_unlock(&mutex_);
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
    pthread_mutex_lock(&mutex_);
    const uint32_t tenants = tenants_.size();
    pthread_mutex_unlock(&mutex_);
    return tenants;
}

bool DeviceTenants::vacate(uint32_t wait_ms)
{
    AV_SCOPE("DeviceTenants::vacate");
    constexpr uint64_t nanoseconds_per_ms = 1000000u;
    const timespec     deadline = clock_.ahead(static_cast<uint64_t>(wait_ms) * nanoseconds_per_ms);
    pthread_mutex_lock(&mutex_);
    int waited = 0;
    while (tenants_.size() != 0u && waited == 0) waited = pthread_cond_timedwait(&left_, &mutex_, &deadline);
    const bool empty = tenants_.size() == 0u;
    pthread_mutex_unlock(&mutex_);
    return empty;
}

Status DeviceTenants::renew()
{
    AV_SCOPE("DeviceTenants::renew");
    pthread_mutex_lock(&mutex_);
    const Status renewed = !lost_ || tenants_.size() != 0u ? Status::invalid_state : Status::ok;
    if (renewed == Status::ok) __atomic_store_n(&lost_, false, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&mutex_);
    return renewed;
}

/* Every listed tenant is a signal, listed once. */
bool DeviceTenants::invariants_hold() const
{
    AV_SCOPE("DeviceTenants::invariants_hold");
    pthread_mutex_lock(&mutex_);
    bool holds = true;
    for (uint32_t index = 0u; holds && index < tenants_.size(); index++) {
        holds = tenants_.at(index) != nullptr;
        for (uint32_t other = index + 1u; holds && other < tenants_.size(); other++)
            holds = tenants_.at(index) != tenants_.at(other);
    }
    pthread_mutex_unlock(&mutex_);
    return holds;
}

}  // namespace av
