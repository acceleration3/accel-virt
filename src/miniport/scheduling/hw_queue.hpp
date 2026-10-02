/* One hardware queue dxgkrnl created on a context: its address is the hHwQueue
 * the driver answers and dxgkrnl passes back, and it holds the location
 * dxgkrnl allocated for the queue's progress - read-only to the process that
 * owns the queue, and what the driver writes what the device has finished into
 * once that process submits for itself (feature 049). */
#ifndef AV_HW_QUEUE_HPP
#define AV_HW_QUEUE_HPP

#include "core/platform/platform.hpp"

#include <stdint.h>

namespace av {

class Context;

class HwQueue {
public:
    HwQueue(Platform &platform, Context &context, uint64_t *progress);
    HwQueue(const HwQueue &) = delete;
    HwQueue &operator=(const HwQueue &) = delete;

    Context  &context() const;
    uint64_t *progress() const;

private:
    Platform &platform_;
    Context  &context_;
    uint64_t *progress_;
};

}  // namespace av

#endif  // AV_HW_QUEUE_HPP
