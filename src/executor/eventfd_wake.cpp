#include "executor/eventfd_wake.hpp"

#include "core/trace/function_scope.hpp"

#include <poll.h>
#include <sys/eventfd.h>
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

EventfdWake::EventfdWake() : signal_(), reached_fd_(-1), stop_fd_(-1), thread_{}, running_(false)
{
    AV_SCOPE("EventfdWake::EventfdWake");
}

EventfdWake::~EventfdWake()
{
    AV_SCOPE("EventfdWake::~EventfdWake");
    stop();
    if (reached_fd_ >= 0) close(reached_fd_);
    if (stop_fd_ >= 0) close(stop_fd_);
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
    if (reached_fd_ > 0 && stop_fd_ >= 0) return true;
    reached_fd_ = eventfd(0u, EFD_CLOEXEC | EFD_NONBLOCK);
    stop_fd_ = eventfd(0u, EFD_CLOEXEC | EFD_NONBLOCK);
    if (reached_fd_ > 0 && stop_fd_ >= 0) return true;
    if (reached_fd_ >= 0) close(reached_fd_);
    if (stop_fd_ >= 0) close(stop_fd_);
    reached_fd_ = -1;
    stop_fd_ = -1;
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

void EventfdWake::unbind()
{
    AV_SCOPE("EventfdWake::unbind");
    stop();
    signal_.clear();
}

void EventfdWake::run()
{
    AV_SCOPE("EventfdWake::run");
    pollfd watched[2] = {{reached_fd_, POLLIN, 0}, {stop_fd_, POLLIN, 0}};
    for (;;) {
        if (poll(watched, 2u, -1) < 0) continue;
        if (watched[1].revents != 0) return;
        uint64_t count = 0u;
        if (read(reached_fd_, &count, sizeof count) == sizeof count) static_cast<void>(signal_.raise());
    }
}

}
