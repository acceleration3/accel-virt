#ifndef AV_RETIRED_SLOTS_HPP
#define AV_RETIRED_SLOTS_HPP

#include "executor/d3d/queue_drain.hpp"

#include <stdint.h>

namespace av {

class IdleQueue;

class RetiredSlots {
public:
    static constexpr uint32_t capacity = 4u;

    RetiredSlots();
    RetiredSlots(const RetiredSlots &) = delete;
    RetiredSlots &operator=(const RetiredSlots &) = delete;

    [[nodiscard]] bool retire(uint32_t slot, IdleQueue &queue);
    [[nodiscard]] bool done(uint32_t slot, const IdleQueue &queue) const;
    void               freed(uint32_t slot);

    bool retired(uint32_t slot) const;
    bool any() const;

private:
    QueueDrain drains_[capacity];
};

}

#endif
