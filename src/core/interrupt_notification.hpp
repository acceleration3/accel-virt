/* A completion reported to dxgkrnl from outside the ISR: the notify-interrupt
 * data is built once, and DxgkCbNotifyInterrupt is called from a routine
 * DxgkCbSynchronizeExecution runs at device IRQL - the recipe basicrender.sys
 * uses (WARPKMADAPTER::WarpSynchronizeNotifyInterrupt), with a data block
 * reused for every report rather than allocated. Reporters that can run at
 * once - the adapter's DPC and the vsync timer - each own one. */
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
    /* Hardware queue progress fences of the node's engine were written. */
    [[nodiscard]] Status report_monitored_fence_signaled(uint32_t node, uint32_t engine);
    /* The address is the primary the target shows at this vsync. */
    [[nodiscard]] Status report_vsync(uint32_t target, uint64_t address);
    /* The synchronized routine: runs at device IRQL. */
    uint8_t              notify();

private:
    enum : int32_t { dma_completed = 1, dma_preempted = 2, crtc_vsync = 3, monitored_fence_signaled = 11 };   /* DXGK_INTERRUPT_TYPE */
    /* One adapter, not linked: physical adapter 0. */
    static constexpr uint32_t physical_adapter_mask = 1u;

    [[nodiscard]] Status report();

    Platform                          &platform_;
    DxgkrnlInterface                  &dxgkrnl_;
    DxgkargcbNotifyInterruptDataRecord data_;
};

}  // namespace av

#endif  // AV_INTERRUPT_NOTIFICATION_HPP
