/* A completion reported to dxgkrnl from outside the ISR: the notify-interrupt
 * data is built once, and DxgkCbNotifyInterrupt is called from a routine
 * DxgkCbSynchronizeExecution runs at device IRQL - the recipe basicrender.sys
 * uses (WARPKMADAPTER::WarpSynchronizeNotifyInterrupt), with a data block
 * reused for every report rather than allocated. */
#ifndef AV_INTERRUPT_NOTIFICATION_HPP
#define AV_INTERRUPT_NOTIFICATION_HPP

#include "ddi/dxgkargcb_notify_interrupt_data_record.hpp"
#include "platform.hpp"
#include "status.hpp"

#include <stdint.h>

namespace av {

class DxgkrnlInterface;

class InterruptNotification {
public:
    InterruptNotification(Platform &platform, DxgkrnlInterface &dxgkrnl);
    InterruptNotification(const InterruptNotification &) = delete;
    InterruptNotification &operator=(const InterruptNotification &) = delete;

    [[nodiscard]] Status report_completed(uint32_t fence);
    [[nodiscard]] Status report_preempted(uint32_t preemption_fence, uint32_t last_completed_fence);
    /* The synchronized routine: runs at device IRQL. */
    uint8_t              notify();

private:
    enum : int32_t { dma_completed = 1, dma_preempted = 2 };   /* DXGK_INTERRUPT_TYPE */

    [[nodiscard]] Status report();

    Platform                          &platform_;
    DxgkrnlInterface                  &dxgkrnl_;
    DxgkargcbNotifyInterruptDataRecord data_;
};

}  // namespace av

#endif  // AV_INTERRUPT_NOTIFICATION_HPP
