#include "core/miniport/interrupt_notification.hpp"

#include "core/ddi/entry_points.hpp"
#include "core/miniport/dxgkrnl_interface.hpp"

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

/* MonitoredFenceSignaled's NodeOrdinal and EngineOrdinal are the first two UINTs. */
Status InterruptNotification::report_monitored_fence_signaled(uint32_t node, uint32_t engine)
{
    platform_.require_irql_at_most(Irql::dispatch, "InterruptNotification::report_monitored_fence_signaled");
    data_ = DxgkargcbNotifyInterruptDataRecord{};
    data_.InterruptType = monitored_fence_signaled;
    data_.FenceId = node;
    data_.Argument1 = engine;
    return report();
}

/* CRTC_VSYNC's PhysicalAddress spans the two UINTs after the target. */
Status InterruptNotification::report_vsync(uint32_t target, uint64_t address)
{
    platform_.require_irql_at_most(Irql::dispatch, "InterruptNotification::report_vsync");
    data_ = DxgkargcbNotifyInterruptDataRecord{};
    data_.InterruptType = crtc_vsync;
    data_.FenceId = target;
    data_.Argument2 = static_cast<uint32_t>(address);
    data_.Argument3 = static_cast<uint32_t>(address >> 32u);
    data_.UnionRemainder[0] = physical_adapter_mask;
    return report();
}

uint8_t InterruptNotification::notify()
{
    platform_.require_irql_at_most(Irql::isr, "InterruptNotification::notify");
    dxgkrnl_.notify_interrupt(data_);
    return 1u;
}

}  // namespace av
