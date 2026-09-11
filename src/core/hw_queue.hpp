/* One hardware queue dxgkrnl created on a context: its address is the hHwQueue
 * the driver answers and dxgkrnl passes back. */
#ifndef AV_HW_QUEUE_HPP
#define AV_HW_QUEUE_HPP

#include "platform.hpp"

namespace av {

class Context;

class HwQueue {
public:
    HwQueue(Platform &platform, Context &context);
    HwQueue(const HwQueue &) = delete;
    HwQueue &operator=(const HwQueue &) = delete;

    Context &context() const;

private:
    Platform &platform_;
    Context  &context_;
};

}  // namespace av

#endif  // AV_HW_QUEUE_HPP
