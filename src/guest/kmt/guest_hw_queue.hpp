#ifndef AV_GUEST_HW_QUEUE_HPP
#define AV_GUEST_HW_QUEUE_HPP

#include <stdint.h>

namespace av {

class GuestHwQueue {
public:
    GuestHwQueue();
    GuestHwQueue(uint32_t handle, uint32_t progress_fence, const uint64_t *progress);

    uint32_t handle() const;
    uint32_t progress_fence() const;
    uint64_t progress() const;
    bool     established() const;

private:
    uint32_t        handle_;
    uint32_t        progress_fence_;
    const uint64_t *progress_;
};

}

#endif
