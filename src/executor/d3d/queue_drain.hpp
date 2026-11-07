#ifndef AV_QUEUE_DRAIN_HPP
#define AV_QUEUE_DRAIN_HPP

#include <stdint.h>

namespace av {

class IdleQueue;

class QueueDrain {
public:
    QueueDrain();
    QueueDrain(const QueueDrain &) = delete;
    QueueDrain &operator=(const QueueDrain &) = delete;

    [[nodiscard]] bool mark(IdleQueue &queue, uint64_t work);
    [[nodiscard]] bool reached(const IdleQueue &queue, uint64_t nanoseconds) const;
    void               forget();

    bool marked() const;

private:
    uint64_t ticket_;
    uint64_t work_;
    bool     marked_;
};

}

#endif
