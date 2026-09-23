#include "core/miniport/interrupt_notification.hpp"

#include "core/ddi/entry_points.hpp"
#include "core/miniport/dxgkrnl_interface.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

InterruptNotification::InterruptNotification(Platform &platform, DxgkrnlInterface &dxgkrnl)
    : platform_(platform), dxgkrnl_(dxgkrnl), data_()
    {
        AV_SCOPE("InterruptNotification::InterruptNotification");
    }

Status InterruptNotification::report()
{
    AV_SCOPE("InterruptNotification::report");
    return dxgkrnl_.synchronize_execution(&AvSynchronizedNotify, this);
}

Status InterruptNotification::report_completed(uint32_t fence)
{
    AV_SCOPE("InterruptNotification::report_completed");
    platform_.require_irql_at_most(Irql::dispatch, "InterruptNotification::report_completed");
    data_ = DxgkargcbNotifyInterruptDataRecord{};
    data_.InterruptType = dma_completed;
    data_.FenceId = fence;
    return report();
}

Status InterruptNotification::report_preempted(uint32_t preemption_fence, uint32_t last_completed_fence)
{
    AV_SCOPE("InterruptNotification::report_preempted");
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
    AV_SCOPE("InterruptNotification::report_monitored_fence_signaled");
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
    AV_SCOPE("InterruptNotification::report_vsync");
    platform_.require_irql_at_most(Irql::dispatch, "InterruptNotification::report_vsync");
    data_ = DxgkargcbNotifyInterruptDataRecord{};
    data_.InterruptType = crtc_vsync;
    data_.FenceId = target;
    data_.Argument2 = static_cast<uint32_t>(address);
    data_.Argument3 = static_cast<uint32_t>(address >> 32u);
    data_.UnionRemainder[0] = physical_adapter_mask;
    return report();
}

/* CrtcVsyncWithMultiPlaneOverlay: the target, the physical adapter mask and
 * the layer count as the first three UINTs, then the layers' address at the
 * next 8-byte boundary. */
Status InterruptNotification::report_multiplane_vsync(uint32_t target, const DxgkMultiplaneOverlayVsyncInfoRecord *layers,
                                                      uint32_t count)
{
    AV_SCOPE("InterruptNotification::report_multiplane_vsync");
    platform_.require_irql_at_most(Irql::dispatch, "InterruptNotification::report_multiplane_vsync");
    data_ = DxgkargcbNotifyInterruptDataRecord{};
    data_.InterruptType = crtc_vsync_with_multiplane_overlay;
    data_.FenceId = target;
    data_.Argument1 = physical_adapter_mask;
    data_.Argument2 = layers != nullptr ? count : 0u;
    const auto address = reinterpret_cast<uintptr_t>(layers);
    data_.UnionRemainder[0] = static_cast<uint32_t>(address);
    data_.UnionRemainder[1] = static_cast<uint32_t>(static_cast<uint64_t>(address) >> 32u);
    return report();
}

uint8_t InterruptNotification::notify()
{
    AV_SCOPE("InterruptNotification::notify");
    platform_.require_irql_at_most(Irql::isr, "InterruptNotification::notify");
    dxgkrnl_.notify_interrupt(data_);
    return 1u;
}

}  // namespace av
