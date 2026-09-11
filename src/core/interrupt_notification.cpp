#include "interrupt_notification.hpp"

#include "ddi/entry_points.hpp"
#include "dxgkrnl_interface.hpp"

namespace av {

InterruptNotification::InterruptNotification(Platform &platform, DxgkrnlInterface &dxgkrnl)
    : platform_(platform), dxgkrnl_(dxgkrnl), data_() {}

Status InterruptNotification::report()
{
    return dxgkrnl_.synchronize_execution(&AvSynchronizedNotify, this);
}

Status InterruptNotification::report_completed(uint32_t fence)
{
    platform_.require_irql_at_most(Irql::dispatch, "InterruptNotification::report_completed");
    data_ = DxgkargcbNotifyInterruptDataRecord{};
    data_.InterruptType = dma_completed;
    data_.FenceId = fence;
    return report();
}

Status InterruptNotification::report_preempted(uint32_t preemption_fence, uint32_t last_completed_fence)
{
    platform_.require_irql_at_most(Irql::dispatch, "InterruptNotification::report_preempted");
    data_ = DxgkargcbNotifyInterruptDataRecord{};
    data_.InterruptType = dma_preempted;
    data_.FenceId = preemption_fence;
    data_.Argument1 = last_completed_fence;
    return report();
}

uint8_t InterruptNotification::notify()
{
    platform_.require_irql_at_most(Irql::isr, "InterruptNotification::notify");
    dxgkrnl_.notify_interrupt(data_);
    return 1u;
}

}  // namespace av
