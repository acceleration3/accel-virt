/* The vertical blanks dxgkrnl paces the display to, as the device's display
 * clock counts them (docs/protocol.md, "Display clock"). The driver registers
 * a page with the device; the device writes each blank there on the host's
 * clock and raises the display line on the blanks the driver asked for, and
 * the DPC that follows reports a vsync for the flip the device latched at that
 * blank - so a flip is retired when it is on the host's screen, and a DPC that
 * runs late loses no blank, since the count is the device's.
 *
 * While dxgkrnl waits for vsync the device raises the line on every blank;
 * otherwise on every sweep_every-th, which is also how often the DPC runs its
 * sweeps (ExecutionNode::release_overdue) when nothing else wakes it. */
#ifndef AV_DEVICE_VBLANK_HPP
#define AV_DEVICE_VBLANK_HPP

#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info_record.hpp"
#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info2_record.hpp"
#include "miniport/display/clock_page_view.hpp"
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

class DeviceVblank {
public:
    /* ~400 ms at 60 Hz. */
    static constexpr uint32_t sweep_every = 24u;
    /* Blanks between the lines that say how the clock is keeping. */
    static constexpr uint32_t blanks_per_report = 120u;

    DeviceVblank(Platform &platform, DxgkrnlInterface &dxgkrnl, DisplaySources &sources);
    DeviceVblank(const DeviceVblank &) = delete;
    DeviceVblank &operator=(const DeviceVblank &) = delete;

    /* StartDevice: the page. */
    [[nodiscard]] Status initialize();
    /* StopDevice: nothing reported, the page freed. */
    void                 destroy();
    /* PASSIVE, on a device that has just started: the page registered with it
     * and the interrupts dxgkrnl wants asked for. invalid_state without a
     * page; the device's refusal otherwise, which leaves dxgkrnl with no
     * vsync from this device and is the caller's to log. */
    [[nodiscard]] Status attach(VirtioGpuDevice &device);
    /* ControlInterrupt. invalid_state before StartDevice. */
    [[nodiscard]] Status enable();
    void                 disable();
    /* Power went away: nothing is reported until resume. dxgkrnl keeps asking
     * for vertical blanks across a suspend, and one reported to an adapter
     * that is not powered - or to one on its way out of S3, before dxgkrnl has
     * powered it up - bugchecks the guest. */
    void                 suspend();
    /* Power is back and the page registered again: the reporting dxgkrnl
     * asked for is put back. */
    [[nodiscard]] Status resume();
    /* The DPC, at DISPATCH: a blank the page shows that was not seen before
     * is reported, if dxgkrnl asked, as a vsync of the flip on screen - the
     * one the device latched when it shows posted frames at its blanks
     * (`latching`), the newest when it shows them as they come. `now` is the
     * interrupt time. */
    void                 tick(uint64_t now, bool latching);
    /* The line `mode` is scanning at `now` (interrupt time), from the blanks'
     * phase; a mode's own reckoning until a blank has been seen. */
    uint32_t             scan_line(const DisplayMode &mode, uint64_t now) const;
    /* Vertical blanks reported since the last line said. */
    uint32_t             reported() const;
    /* Times dxgkrnl asked for them since then. */
    uint32_t             enabled_count() const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t target = 0u;   /* shows source 0 */
    /* DISPLAY_CLOCK's answer: a header, nothing more. */
    static constexpr size_t   response_bytes = 24u;

    /* interrupt_every as reporting stands. */
    void                 ask();
    [[nodiscard]] Status report(const FlipView &shown);
    void                 report_rate(uint64_t now);

    Platform             &platform_;
    DisplaySources       &sources_;
    InterruptNotification notification_;
    ClockPageView         page_;
    VblankPhase           phase_;
    /* The layer a multiplane vsync reports, kept here because dxgkrnl reads
     * it through the interrupt's pointer. */
    DxgkMultiplaneOverlayVsyncInfoRecord layer_;
    /* Likewise the layer a vsync of an MPO3 flip reports. */
    DxgkMultiplaneOverlayVsyncInfo2Record presented_layer_;
    bool                  reporting_;
    bool                  suspended_;
    /* What reporting_ was when power went away, to put back on the way up. */
    bool                  suspended_reporting_;
    /* The newest blank seen: written by the DPC, and by enable at PASSIVE,
     * so atomically. Then the count the rate line is kept over. */
    uint64_t              sequence_;
    uint32_t              blanks_;
    uint32_t              missed_;
    uint32_t              reported_;
    uint32_t              enables_;
    uint64_t              window_start_;
};

}  // namespace av

#endif  // AV_DEVICE_VBLANK_HPP
