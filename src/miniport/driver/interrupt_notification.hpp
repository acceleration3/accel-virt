#ifndef AV_INTERRUPT_NOTIFICATION_HPP
#define AV_INTERRUPT_NOTIFICATION_HPP

#include "miniport/ddi/dxgkargcb_notify_interrupt_data_record.hpp"
#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class DxgkrnlInterface;
struct DxgkMultiplaneOverlayVsyncInfoRecord;
struct DxgkMultiplaneOverlayVsyncInfo2Record;

class InterruptNotification {
public:
    InterruptNotification(Platform &platform, DxgkrnlInterface &dxgkrnl);
    InterruptNotification(const InterruptNotification &) = delete;
    InterruptNotification &operator=(const InterruptNotification &) = delete;

    [[nodiscard]] Status report_completed(uint32_t fence);
    [[nodiscard]] Status report_preempted(uint32_t preemption_fence, uint32_t last_completed_fence);
    [[nodiscard]] Status report_monitored_fence_signaled(uint32_t node, uint32_t engine);
    [[nodiscard]] Status report_vsync(uint32_t target, uint64_t address);
    [[nodiscard]] Status report_multiplane_vsync(uint32_t target, const DxgkMultiplaneOverlayVsyncInfoRecord *layers,
                                                 uint32_t count);
    [[nodiscard]] Status report_presented_vsync(uint32_t target, const DxgkMultiplaneOverlayVsyncInfo2Record *layers,
                                                uint32_t count);
    uint8_t              notify();

private:
    enum : int32_t {
        dma_completed = 1,
        dma_preempted = 2,
        crtc_vsync = 3,
        crtc_vsync_with_multiplane_overlay = 7,
        crtc_vsync_with_multiplane_overlay2 = 10,
        monitored_fence_signaled = 11
    };
    static constexpr uint32_t physical_adapter_mask = 1u;

    [[nodiscard]] Status report();

    Platform                          &platform_;
    DxgkrnlInterface                  &dxgkrnl_;
    DxgkargcbNotifyInterruptDataRecord data_;
};

}

#endif
