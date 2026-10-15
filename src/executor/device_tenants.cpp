#include "executor/device_tenants.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/wake_signal.hpp"

#include <time.h>

namespace av {

DeviceTenants::DeviceTenants() : mutex_(), left_(), tenants_{}, count_(0u), lost_(false)
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
    Status joined = lost_ ? Status::invalid_state : count_ == capacity ? Status::no_space : Status::ok;
    for (uint32_t index = 0u; joined == Status::ok && index < count_; index++)
        if (tenants_[index] == &wake) joined = Status::conflict;
    if (joined == Status::ok) tenants_[count_++] = &wake;
    pthread_mutex_unlock(&mutex_);
    return joined;
}

void DeviceTenants::leave(WakeSignal &wake)
{
    AV_SCOPE("DeviceTenants::leave");
    pthread_mutex_lock(&mutex_);
    for (uint32_t index = 0u; index < count_; index++) {
        if (tenants_[index] != &wake) continue;
        tenants_[index] = tenants_[--count_];
        tenants_[count_] = nullptr;
        break;
    }
    if (count_ == 0u) pthread_cond_broadcast(&left_);
    pthread_mutex_unlock(&mutex_);
}

bool DeviceTenants::lose()
{
    AV_SCOPE("DeviceTenants::lose");
    pthread_mutex_lock(&mutex_);
    const bool first = !lost_;
    __atomic_store_n(&lost_, true, __ATOMIC_RELEASE);
    if (first)
        for (uint32_t index = 0u; index < count_; index++) static_cast<void>(tenants_[index]->raise());
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
    const uint32_t tenants = count_;
    pthread_mutex_unlock(&mutex_);
    return tenants;
}

bool DeviceTenants::vacate(uint32_t wait_ms)
{
    AV_SCOPE("DeviceTenants::vacate");
    constexpr long nanoseconds_per_second = 1000000000;
    timespec       deadline = {};
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += static_cast<time_t>(wait_ms / 1000u);
    deadline.tv_nsec += static_cast<long>(wait_ms % 1000u) * 1000000;
    if (deadline.tv_nsec >= nanoseconds_per_second) {
        deadline.tv_sec++;
        deadline.tv_nsec -= nanoseconds_per_second;
    }
    pthread_mutex_lock(&mutex_);
    int waited = 0;
    while (count_ != 0u && waited == 0) waited = pthread_cond_timedwait(&left_, &mutex_, &deadline);
    const bool empty = count_ == 0u;
    pthread_mutex_unlock(&mutex_);
    return empty;
}

Status DeviceTenants::renew()
{
    AV_SCOPE("DeviceTenants::renew");
    pthread_mutex_lock(&mutex_);
    const Status renewed = !lost_ || count_ != 0u ? Status::invalid_state : Status::ok;
    if (renewed == Status::ok) __atomic_store_n(&lost_, false, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&mutex_);
    return renewed;
}

/* Every listed tenant is a signal, listed once, and nothing past the count. */
bool DeviceTenants::invariants_hold() const
{
    AV_SCOPE("DeviceTenants::invariants_hold");
    pthread_mutex_lock(&mutex_);
    bool holds = count_ <= capacity;
    for (uint32_t index = 0u; holds && index < capacity; index++) {
        holds = (index < count_) == (tenants_[index] != nullptr);
        for (uint32_t other = index + 1u; holds && other < count_; other++) holds = tenants_[index] != tenants_[other];
    }
    pthread_mutex_unlock(&mutex_);
    return holds;
}

}  // namespace av
