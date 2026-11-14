#ifndef AV_DEVICE_VBLANK_HPP
#define AV_DEVICE_VBLANK_HPP

#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info_record.hpp"
#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info2_record.hpp"
#include "miniport/display/clock_page_view.hpp"
#include "miniport/display/overlay_plane.hpp"
#include "miniport/display/vblank_phase.hpp"
#include "miniport/driver/interrupt_notification.hpp"
#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class DisplayMode;
class DisplaySources;
class DxgkrnlInterface;
class FlipView;
class VirtioGpuDevice;
struct DeviceVblankProbe;

class DeviceVblank {
public:
    static constexpr uint32_t sweep_every = 24u;

    DeviceVblank(Platform &platform, DxgkrnlInterface &dxgkrnl, DisplaySources &sources);
    DeviceVblank(const DeviceVblank &) = delete;
    DeviceVblank &operator=(const DeviceVblank &) = delete;

    [[nodiscard]] Status initialize();
    void                 destroy();
    [[nodiscard]] Status attach(VirtioGpuDevice &device);
    [[nodiscard]] Status enable();
    void                 disable();
    void                 suspend();
    [[nodiscard]] Status resume();
    void                 tick(uint64_t now, bool latching);
    uint32_t             scan_line(const DisplayMode &mode, uint64_t now) const;
    uint32_t             reported() const;
    uint32_t             enabled_count() const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t target = 0u;
    static constexpr size_t   response_bytes = 24u;

    void                 ask();
    void                 report_now();
    [[nodiscard]] Status report(const FlipView &shown);

    Platform             &platform_;
    DisplaySources       &sources_;
    InterruptNotification notification_;
    ClockPageView         page_;
    VblankPhase           phase_;
    DxgkMultiplaneOverlayVsyncInfoRecord layer_;
    DxgkMultiplaneOverlayVsyncInfo2Record presented_layers_[OverlayPlane::max_planes];
    bool                  reporting_;
    bool                  suspended_;
    bool                  suspended_reporting_;
    uint64_t              sequence_;
    uint32_t              reported_;
    uint32_t              enables_;
    uint32_t              reported_frame_ = 0u;

    friend struct DeviceVblankProbe;
};

}

#endif
