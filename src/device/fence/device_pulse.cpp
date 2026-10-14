#include "device/fence/device_pulse.hpp"

#include "core/trace/function_scope.hpp"
#include "device/abi/entry_points.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/host/host.hpp"

namespace av {

DevicePulse::DevicePulse(Host &host)
    : host_(host), lock_(nullptr), wake_(nullptr), thread_(nullptr), stopping_(false), dormant_(false), carried_{}
{
    AV_SCOPE("DevicePulse::DevicePulse");
}

DevicePulse::~DevicePulse()
{
    AV_SCOPE("DevicePulse::~DevicePulse");
    stop_thread();
    if (wake_ != nullptr) host_.destroy_event(wake_);
    if (lock_ != nullptr) host_.destroy_mutex(lock_);
}

Status DevicePulse::initialize()
{
    AV_SCOPE("DevicePulse::initialize");
    host_.require_main_loop("DevicePulse::initialize");
    if (lock_ != nullptr) return Status::invalid_state;
    Status status = host_.create_mutex(lock_);
    if (status == Status::ok) status = host_.create_event(wake_);
    if (status == Status::ok) return Status::ok;
    if (lock_ != nullptr) host_.destroy_mutex(lock_);
    lock_ = nullptr;
    return status;
}

Status DevicePulse::carry(FenceReporter &reporter)
{
    AV_SCOPE("DevicePulse::carry");
    host_.require_main_loop("DevicePulse::carry");
    if (lock_ == nullptr) return Status::invalid_state;
    if (thread_ == nullptr) {
        stopping_ = false;
        if (host_.start_thread(&AvDevicePulseRun, this, thread_) != Status::ok) {
            thread_ = nullptr;
            return Status::no_memory;
        }
    }
    host_.lock_mutex(lock_);
    reporter.beat_by(*this);
    FenceReporter **free_slot = nullptr;
    Status          status = Status::no_space;
    for (FenceReporter *&slot : carried_) {
        if (slot == &reporter) status = Status::invalid_state;
        if (slot == nullptr && free_slot == nullptr) free_slot = &slot;
    }
    if (status != Status::invalid_state && free_slot != nullptr) {
        *free_slot = &reporter;
        status = Status::ok;
    }
    host_.unlock_mutex(lock_);
    return status;
}

void DevicePulse::drop(FenceReporter &reporter)
{
    AV_SCOPE("DevicePulse::drop");
    host_.require_main_loop("DevicePulse::drop");
    if (lock_ == nullptr) return;
    host_.lock_mutex(lock_);
    for (FenceReporter *&slot : carried_)
        if (slot == &reporter) slot = nullptr;
    host_.unlock_mutex(lock_);
    if (carried() == 0u) stop_thread();
}

/* Paired with rest: the worker's store that it works comes before this load,
 * and the pulse's store that it is dormant before its look at the workers,
 * both sequentially consistent, so one of the two always sees the other. */
void DevicePulse::stir()
{
    AV_SCOPE("DevicePulse::stir");
    if (!__atomic_load_n(&dormant_, __ATOMIC_SEQ_CST)) return;
    if (__atomic_exchange_n(&dormant_, false, __ATOMIC_SEQ_CST)) host_.signal_event(wake_);
}

uint32_t DevicePulse::carried() const
{
    AV_SCOPE("DevicePulse::carried");
    uint32_t count = 0u;
    for (const FenceReporter *slot : carried_) count += slot != nullptr ? 1u : 0u;
    return count;
}

void DevicePulse::run()
{
    AV_SCOPE("DevicePulse::run");
    bool working = false;
    while (tick(working)) {
        if (working) static_cast<void>(host_.wait_event_until(wake_, host_.monotonic_nanoseconds() + period_ns));
        else if (rest()) host_.wait_event(wake_);
    }
}

/* Under the lock a page cannot be dropped, and so cannot go, while it is
 * beaten. */
bool DevicePulse::tick(bool &working)
{
    AV_SCOPE("DevicePulse::tick");
    host_.lock_mutex(lock_);
    const bool going = !stopping_;
    working = any_working();
    if (going && working)
        for (FenceReporter *slot : carried_)
            if (slot != nullptr) slot->beat();
    host_.unlock_mutex(lock_);
    return going;
}

bool DevicePulse::rest()
{
    AV_SCOPE("DevicePulse::rest");
    host_.lock_mutex(lock_);
    __atomic_store_n(&dormant_, true, __ATOMIC_SEQ_CST);
    const bool idle = !any_working() && !stopping_;
    if (!idle) __atomic_store_n(&dormant_, false, __ATOMIC_SEQ_CST);
    host_.unlock_mutex(lock_);
    return idle;
}

bool DevicePulse::any_working() const
{
    AV_SCOPE("DevicePulse::any_working");
    for (const FenceReporter *slot : carried_)
        if (slot != nullptr && slot->working()) return true;
    return false;
}

void DevicePulse::stop_thread()
{
    AV_SCOPE("DevicePulse::stop_thread");
    if (thread_ == nullptr) return;
    host_.lock_mutex(lock_);
    stopping_ = true;
    host_.unlock_mutex(lock_);
    host_.signal_event(wake_);
    host_.join_thread(thread_);
    thread_ = nullptr;
}

/* A page is carried once, and a thread runs only for a pulse that has a lock
 * and an event to run on. */
bool DevicePulse::invariants_hold() const
{
    AV_SCOPE("DevicePulse::invariants_hold");
    if (thread_ != nullptr && (lock_ == nullptr || wake_ == nullptr)) return false;
    for (uint32_t first = 0u; first < capacity; first++)
        for (uint32_t second = first + 1u; second < capacity; second++)
            if (carried_[first] != nullptr && carried_[first] == carried_[second]) return false;
    return true;
}

}  // namespace av
