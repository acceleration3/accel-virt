#include "executor/eventfd_wake.hpp"

#include "core/trace/function_scope.hpp"

#include <poll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

namespace av {

namespace {

void *run_eventfd_wake(void *wake)
{
    AV_SCOPE("run_eventfd_wake");
    static_cast<EventfdWake *>(wake)->run();
    return nullptr;
}

}

EventfdWake::EventfdWake() : signal_(), reached_fd_(-1), stop_fd_(-1), timer_fd_(-1), retry_delay_(0u), retry_armed_(false), thread_{}, running_(false)
{
    AV_SCOPE("EventfdWake::EventfdWake");
}

EventfdWake::~EventfdWake()
{
    AV_SCOPE("EventfdWake::~EventfdWake");
    stop();
    if (reached_fd_ >= 0) close(reached_fd_);
    if (stop_fd_ >= 0) close(stop_fd_);
    if (timer_fd_ >= 0) close(timer_fd_);
}

void EventfdWake::bind(void (*signal)(void *context), void *context)
{
    AV_SCOPE("EventfdWake::bind");
    signal_.bind(signal, context);
}

bool EventfdWake::bound() const
{
    AV_SCOPE("EventfdWake::bound");
    return signal_.bound();
}

int EventfdWake::descriptor()
{
    AV_SCOPE("EventfdWake::descriptor");
    if (!running_ && !start()) return -1;
    return reached_fd_;
}

bool EventfdWake::open()
{
    AV_SCOPE("EventfdWake::open");
    if (reached_fd_ > 0 && stop_fd_ >= 0 && timer_fd_ >= 0) return true;
    reached_fd_ = eventfd(0u, EFD_CLOEXEC | EFD_NONBLOCK);
    stop_fd_ = eventfd(0u, EFD_CLOEXEC | EFD_NONBLOCK);
    timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (reached_fd_ > 0 && stop_fd_ >= 0 && timer_fd_ >= 0) return true;
    if (reached_fd_ >= 0) close(reached_fd_);
    if (stop_fd_ >= 0) close(stop_fd_);
    if (timer_fd_ >= 0) close(timer_fd_);
    reached_fd_ = -1;
    stop_fd_ = -1;
    timer_fd_ = -1;
    return false;
}

bool EventfdWake::start()
{
    AV_SCOPE("EventfdWake::start");
    if (!open()) return false;
    uint64_t stale = 0u;
    static_cast<void>(read(reached_fd_, &stale, sizeof stale));
    running_ = pthread_create(&thread_, nullptr, &run_eventfd_wake, this) == 0;
    return running_;
}

void EventfdWake::stop()
{
    AV_SCOPE("EventfdWake::stop");
    if (!running_) return;
    uint64_t one = 1u;
    static_cast<void>(write(stop_fd_, &one, sizeof one));
    pthread_join(thread_, nullptr);
    static_cast<void>(read(stop_fd_, &one, sizeof one));
    running_ = false;
}

void EventfdWake::retry_later()
{
    AV_SCOPE("EventfdWake::retry_later");
    if (!signal_.bound() || descriptor() < 0 || __atomic_exchange_n(&retry_armed_, true, __ATOMIC_ACQ_REL)) return;
    retry_delay_ = retry_delay_ == 0u ? first_retry_nanoseconds
                   : retry_delay_ >= last_retry_nanoseconds ? last_retry_nanoseconds
                                                            : retry_delay_ * 2u;
    itimerspec due = {};
    due.it_value.tv_sec = static_cast<time_t>(retry_delay_ / 1000000000u);
    due.it_value.tv_nsec = static_cast<long>(retry_delay_ % 1000000000u);
    if (timerfd_settime(timer_fd_, 0, &due, nullptr) < 0) __atomic_store_n(&retry_armed_, false, __ATOMIC_RELEASE);
}

void EventfdWake::settled()
{
    AV_SCOPE("EventfdWake::settled");
    retry_delay_ = 0u;
}

void EventfdWake::unbind()
{
    AV_SCOPE("EventfdWake::unbind");
    stop();
    signal_.clear();
}

void EventfdWake::run()
{
    AV_SCOPE("EventfdWake::run");
    pollfd watched[3] = {{reached_fd_, POLLIN, 0}, {stop_fd_, POLLIN, 0}, {timer_fd_, POLLIN, 0}};
    for (;;) {
        if (poll(watched, 3u, -1) < 0) continue;
        if (watched[1].revents != 0) return;
        uint64_t count = 0u;
        const bool fenced = watched[0].revents != 0 && read(reached_fd_, &count, sizeof count) == sizeof count;
        const bool retried = watched[2].revents != 0 && read(timer_fd_, &count, sizeof count) == sizeof count;
        if (retried) __atomic_store_n(&retry_armed_, false, __ATOMIC_RELEASE);
        if (fenced || retried) static_cast<void>(signal_.raise());
    }
}

}
