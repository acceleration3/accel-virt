#ifndef AV_IDLE_QUEUE_HPP
#define AV_IDLE_QUEUE_HPP

#include <stdint.h>

namespace av {

class IdleQueue {
public:
    IdleQueue(const IdleQueue &) = delete;
    IdleQueue &operator=(const IdleQueue &) = delete;

    [[nodiscard]] virtual bool mark_idle(uint64_t &ticket) = 0;
    [[nodiscard]] virtual bool reached(uint64_t ticket, uint64_t nanoseconds) const = 0;

protected:
    IdleQueue() = default;
    ~IdleQueue() = default;
};

}

#endif
