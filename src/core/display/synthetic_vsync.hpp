/* The vsync of a display with no scan-out interrupt: while enabled, a timer at
 * the refresh rate reports DXGK_INTERRUPT_CRTC_VSYNC for target 0 with the
 * address its source scans out, then asks for dxgkrnl's DPC - what DWM's frame
 * pacing waits on. The notification data is its own, so a tick never races the
 * adapter's DPC. */
#ifndef AV_SYNTHETIC_VSYNC_HPP
#define AV_SYNTHETIC_VSYNC_HPP

#include "core/display/display_mode.hpp"
#include "core/miniport/interrupt_notification.hpp"
#include "core/platform/periodic_timer.hpp"
#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class DisplaySources;
class DxgkrnlInterface;

class SyntheticVsync {
public:
    SyntheticVsync(Platform &platform, DxgkrnlInterface &dxgkrnl, DisplaySources &sources);
    SyntheticVsync(const SyntheticVsync &) = delete;
    SyntheticVsync &operator=(const SyntheticVsync &) = delete;

    /* Creates the timer, stopped: StartDevice. */
    [[nodiscard]] Status initialize();
    [[nodiscard]] Status enable();
    void                 disable();
    /* Waits out a running tick: StopDevice. */
    void                 destroy();
    /* The timer's callback, at DISPATCH. */
    void                 tick();

private:
    static constexpr uint64_t frame_period = 10000000u / DisplayMode::refresh_hz;   /* 100 ns */
    static constexpr uint32_t target = 0u;   /* shows source 0 */

    Platform              &platform_;
    DxgkrnlInterface      &dxgkrnl_;
    DisplaySources        &sources_;
    InterruptNotification  notification_;
    PeriodicTimer          timer_;
};

}  // namespace av

#endif  // AV_SYNTHETIC_VSYNC_HPP
