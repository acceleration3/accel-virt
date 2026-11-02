#ifndef AV_HW_QUEUE_CONTEXTS_HPP
#define AV_HW_QUEUE_CONTEXTS_HPP

#include "core/platform/platform.hpp"
#include "core/platform/spin_lock.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Context;
struct HwQueueContextsProbe;

class HwQueueContexts {
public:
    static constexpr uint32_t capacity = 64u;

    explicit HwQueueContexts(Platform &platform);
    HwQueueContexts(const HwQueueContexts &) = delete;
    HwQueueContexts &operator=(const HwQueueContexts &) = delete;

    [[nodiscard]] Status bind(const void *queue, Context &context);
    void                 forget(const void *queue);
    Context             *context_of(const void *queue);
    uint32_t             bound();

    bool invariants_hold() const;

private:
    Platform   &platform_;
    SpinLock    lock_;
    const void *queues_[capacity];
    Context    *contexts_[capacity];
    uint32_t    count_;

    friend struct HwQueueContextsProbe;
};

}

#endif
