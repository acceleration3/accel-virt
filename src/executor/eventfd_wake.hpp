#ifndef AV_EVENTFD_WAKE_HPP
#define AV_EVENTFD_WAKE_HPP

#include "core/trace/function_scope.hpp"
#include "executor/wake_signal.hpp"

#include <pthread.h>
#include <stdint.h>

namespace av {

class EventfdWake {
public:
    EventfdWake();
    ~EventfdWake();
    EventfdWake(const EventfdWake &) = delete;
    EventfdWake &operator=(const EventfdWake &) = delete;

    void bind(void (*signal)(void *context), void *context);
    template <class Fence>
    [[nodiscard]] bool watch(Fence &fence, uint64_t value);
    void               stop();
    void               unbind();
    void               run();
    int                descriptor();
    [[nodiscard]] bool watch_descriptor(int fd);
    bool               bound() const;
    void               retry_later();
    void               settled();

private:
    static constexpr uint64_t first_retry_nanoseconds = 200000u;
    static constexpr uint64_t last_retry_nanoseconds = 8000000u;

    bool open();
    bool add_to_epoll(int fd, uint32_t events);
    bool start();

    WakeSignal signal_;
    int        reached_fd_;
    int        stop_fd_;
    int        timer_fd_;
    int        epoll_fd_;
    uint64_t   retry_delay_;
    bool       retry_armed_;
    pthread_t  thread_;
    bool       running_;
};

template <class Fence>
bool EventfdWake::watch(Fence &fence, uint64_t value)
{
    AV_SCOPE("EventfdWake::watch");
    if (!signal_.bound()) return true;
    const int event = descriptor();
    return event > 0 && fence.SetEventOnCompletion(value, reinterpret_cast<void *>(static_cast<intptr_t>(event))) >= 0;
}

}

#endif
