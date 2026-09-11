#include "periodic_timer.hpp"

namespace av {

PeriodicTimer::PeriodicTimer(Platform &platform, TimerCallback callback, void *context)
    : platform_(platform), callback_(callback), context_(context), timer_(nullptr) {}

PeriodicTimer::~PeriodicTimer() { destroy(); }

Status PeriodicTimer::initialize()
{
    platform_.require_irql_at_most(Irql::passive, "PeriodicTimer::initialize");
    if (timer_ != nullptr) return Status::invalid_state;
    timer_ = platform_.create_timer(callback_, context_);
    return timer_ != nullptr ? Status::ok : Status::no_memory;
}

Status PeriodicTimer::start(uint64_t period)
{
    platform_.require_irql_at_most(Irql::dispatch, "PeriodicTimer::start");
    if (timer_ == nullptr) return Status::invalid_state;
    if (period == 0u) return Status::invalid_parameter;
    platform_.start_timer(timer_, period);
    return Status::ok;
}

void PeriodicTimer::stop()
{
    platform_.require_irql_at_most(Irql::dispatch, "PeriodicTimer::stop");
    if (timer_ != nullptr) platform_.stop_timer(timer_);
}

void PeriodicTimer::destroy()
{
    platform_.require_irql_at_most(Irql::passive, "PeriodicTimer::destroy");
    if (timer_ == nullptr) return;
    platform_.destroy_timer(timer_);
    timer_ = nullptr;
}

bool PeriodicTimer::created() const { return timer_ != nullptr; }

}  // namespace av
