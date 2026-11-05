#include "executor/eventfd_wake.hpp"

#include "core/trace/function_scope.hpp"

#include <sys/epoll.h>
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

EventfdWake::EventfdWake() : signal_(), reached_fd_(-1), stop_fd_(-1), timer_fd_(-1), epoll_fd_(-1), retry_delay_(0u), retry_armed_(false), thread_{}, running_(false)
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
    if (epoll_fd_ >= 0) close(epoll_fd_);
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
    if (reached_fd_ > 0 && stop_fd_ >= 0 && timer_fd_ >= 0 && epoll_fd_ >= 0) return true;
    reached_fd_ = eventfd(0u, EFD_CLOEXEC | EFD_NONBLOCK);
    stop_fd_ = eventfd(0u, EFD_CLOEXEC | EFD_NONBLOCK);
    timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    const bool opened = reached_fd_ > 0 && stop_fd_ >= 0 && timer_fd_ >= 0 && epoll_fd_ >= 0;
    if (opened && add_to_epoll(reached_fd_, EPOLLIN) && add_to_epoll(stop_fd_, EPOLLIN) &&
        add_to_epoll(timer_fd_, EPOLLIN))
        return true;
    if (reached_fd_ >= 0) close(reached_fd_);
    if (stop_fd_ >= 0) close(stop_fd_);
    if (timer_fd_ >= 0) close(timer_fd_);
    if (epoll_fd_ >= 0) close(epoll_fd_);
    reached_fd_ = -1;
    stop_fd_ = -1;
    timer_fd_ = -1;
    epoll_fd_ = -1;
    return false;
}

bool EventfdWake::add_to_epoll(int fd, uint32_t events)
{
    AV_SCOPE("EventfdWake::add_to_epoll");
    epoll_event added = {};
    added.events = events;
    added.data.fd = fd;
    return epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &added) == 0;
}

bool EventfdWake::watch_descriptor(int fd)
{
    AV_SCOPE("EventfdWake::watch_descriptor");
    if (!signal_.bound()) return true;
    return fd >= 0 && descriptor() > 0 && add_to_epoll(fd, EPOLLIN | EPOLLONESHOT);
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
    constexpr int most_events = 8;
    for (;;) {
        epoll_event events[most_events];
        const int   ready = epoll_wait(epoll_fd_, events, most_events, -1);
        if (ready < 0) continue;
        bool woken = false;
        for (int at = 0; at < ready; at++)
            if (events[at].data.fd == stop_fd_) return;
        for (int at = 0; at < ready; at++) {
            const int fd = events[at].data.fd;
            uint64_t  count = 0u;
            if (fd != reached_fd_ && fd != timer_fd_) {
                woken = true;
                continue;
            }
            if (read(fd, &count, sizeof count) != sizeof count) continue;
            if (fd == timer_fd_) __atomic_store_n(&retry_armed_, false, __ATOMIC_RELEASE);
            woken = true;
        }
        if (woken) static_cast<void>(signal_.raise());
    }
}

}
