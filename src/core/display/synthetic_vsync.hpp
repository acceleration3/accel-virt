/* The vsync of a display with no scan-out interrupt: a timer at the refresh rate
 * reports DXGK_INTERRUPT_CRTC_VSYNC for target 0 with the address its source
 * scans out, then asks for dxgkrnl's DPC - what DWM's frame pacing waits on.
 * The notification data is its own, so a tick never races the adapter's DPC.
 *
 * The timer keeps running from the first enable until the device stops, and
 * only the reporting is gated: dxgkrnl turns vertical blanks on for each
 * WaitForVerticalBlankEvent and off again when the waiter is woken (measured at
 * one enable per blank on a real guest), and re-arming a timer at each enable
 * put every blank a whole period after the wait that asked for it - a real
 * display's blank has a phase of its own, and a waiter saw 30.7 ms instead of
 * 17 (feature 038, US3). The price is a DPC at the refresh rate while the
 * adapter is up. */
#ifndef AV_SYNTHETIC_VSYNC_HPP
#define AV_SYNTHETIC_VSYNC_HPP

#include "core/ddi/dxgk_multiplane_overlay_vsync_info_record.hpp"
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
    /* Power went away: the timer stops, nothing is reported, and the rate
     * window so far is dropped. dxgkrnl keeps asking for vertical blanks across
     * a suspend, and a blank reported to an adapter that is not powered - or to
     * one on its way out of S3, before dxgkrnl has powered it up - bugchecks
     * the guest. */
    void                 suspend();
    /* Power is back: the reporting dxgkrnl asked for is put back, and nothing
     * more. */
    [[nodiscard]] Status resume();
    /* The timer's callback, at DISPATCH. */
    void rearm();
    void                 tick();
    /* Vertical blanks reported since the last line it said. */
    uint32_t             reported() const;
    /* Times dxgkrnl asked for them since then: it turns them off when nothing
     * is waiting, and each turn back on costs a period. */
    uint32_t             enabled_count() const;

private:
    /* Ticks between DPCs when vsync is not being reported: ~400 ms. */
    static constexpr uint32_t ticks_per_sweep = 24u;
    static constexpr uint64_t frame_period = 10000000u / DisplayMode::refresh_hz;   /* 100 ns */
    static constexpr uint32_t target = 0u;   /* shows source 0 */
    /* Two seconds of timer periods at the committed rate: enough to see a rate
     * that is not 60 Hz, and its own budget at one line per two seconds. The
     * window counts periods and not reported blanks, because only the periods
     * are the driver's: dxgkrnl turns the reporting off whenever nothing is
     * waiting, and a window of 120 reported blanks measures how often something
     * waited (a live guest logged 2.3 "blanks" a second for a timer that never
     * left 60 Hz). */
    static constexpr uint32_t ticks_per_report = 120u;

    /* What this vsync tells dxgkrnl the target shows. */
    [[nodiscard]] Status report();
    void                 report_rate();

    Platform              &platform_;
    DxgkrnlInterface      &dxgkrnl_;
    DisplaySources        &sources_;
    InterruptNotification  notification_;
    PeriodicTimer          timer_;
    bool                   reporting_;
    bool                   running_;
    /* What reporting_ was when power went away, to put back on the way up. */
    bool                   suspended_reporting_;
    /* Timer periods since the window opened. */
    /* The layer a multiplane vsync reports, kept here because dxgkrnl reads
     * it through the interrupt's pointer. */
    DxgkMultiplaneOverlayVsyncInfoRecord layer_;
    uint32_t               ticks_;
    uint32_t               reported_;
    uint32_t               enables_;
    uint64_t               window_start_;
    /* When the blanks' schedule began, and how many it has had since. */
    uint64_t               schedule_start_;
    uint64_t               scheduled_;
};

}  // namespace av

#endif  // AV_SYNTHETIC_VSYNC_HPP
