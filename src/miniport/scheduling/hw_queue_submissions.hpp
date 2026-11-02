#ifndef AV_HW_QUEUE_SUBMISSIONS_HPP
#define AV_HW_QUEUE_SUBMISSIONS_HPP

#include "core/platform/platform.hpp"
#include "core/platform/spin_lock.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class InterruptNotification;
struct HwQueueSubmissionsProbe;

class HwQueueSubmissions {
public:
    static constexpr uint32_t capacity = 64u;
    static constexpr uint32_t node = 1u;
    static constexpr uint32_t engine = 0u;

    explicit HwQueueSubmissions(Platform &platform);
    HwQueueSubmissions(const HwQueueSubmissions &) = delete;
    HwQueueSubmissions &operator=(const HwQueueSubmissions &) = delete;

    [[nodiscard]] Status submit(const void *queue, uint64_t fence_id, uint64_t *fence, const void *context,
                                uint64_t awaited);
    void                 completed_through(const void *context, uint64_t completed);
    [[nodiscard]] Status complete(InterruptNotification &notification);
    void                 forget(const void *queue);
    void                 abandon();
    uint32_t             in_flight();

    bool invariants_hold() const;

private:

    Platform  &platform_;
    SpinLock   lock_;
    const void *queues_[capacity];
    uint64_t    fence_ids_[capacity];
    uint64_t   *fences_[capacity];
    const void *contexts_[capacity];
    uint64_t    awaited_[capacity];
    uint32_t    count_;
    bool        written_;

    friend struct HwQueueSubmissionsProbe;
};

}

#endif
