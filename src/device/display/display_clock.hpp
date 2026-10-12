/* The display's vertical blanks, kept by the device (docs/protocol.md, "Display
 * clock"): a thread that wakes at each head's next blank on the host's
 * monotonic clock, latches the newest frame the guest posted for the head,
 * publishes the blank in the guest's page and raises the display line when the
 * guest asked for that blank. What a blank latched is shown on the main loop
 * by the routine the owner gives. The clock runs from the first page the
 * guest registers until a reset. */
#ifndef AV_DISPLAY_CLOCK_HPP
#define AV_DISPLAY_CLOCK_HPP

#include "core/support/status.hpp"
#include "device/display/clock_page_window.hpp"
#include "device/display/frame_latch.hpp"
#include "device/display/vblank_schedule.hpp"
#include "device/host/host.hpp"

#include <stdint.h>

namespace av {

class FenceInterrupts;
struct DisplayClockProbe;

class DisplayClock {
public:
    /* Every head a page describes: the device does not know how many outputs
     * its function has, and a head nothing shows costs a record. */
    static constexpr uint32_t heads = 16u;

    /* `show` runs on the main loop, given `show_context`, after a blank
     * latched a frame. */
    DisplayClock(Host &host, FenceInterrupts &interrupts, HostRoutine show, void *show_context);
    ~DisplayClock();
    DisplayClock(const DisplayClock &) = delete;
    DisplayClock &operator=(const DisplayClock &) = delete;

    /* Main loop. no_memory when the host has no mutex or event; invalid_state
     * once done. */
    [[nodiscard]] Status initialize();
    /* Main loop. The guest's page from now on, and the thread started if it
     * was not: 0 lets the page go and stops publishing. invalid_state before
     * initialize; the page's refusal (ClockPageWindow::map) otherwise, which
     * leaves no page; no_memory when no thread starts. */
    [[nodiscard]] Status attach(uint64_t address);
    /* Main loop. A virtio reset: the thread stopped, the page let go, and
     * every posted and latched frame forgotten. The schedules keep counting,
     * so a page registered again reads sequences that never went back. */
    void                 detach();
    /* Main loop. invalid_parameter for a head past `heads` or a rate the
     * protocol does not allow. */
    [[nodiscard]] Status retime(uint32_t head, uint32_t millihz);
    /* Any thread: the host's display (docs/external-export.md, timing). Every
     * head blanks at `millihz` from its next blank on, in phase with `blank`
     * (VblankSchedule::align). invalid_state before initialize;
     * invalid_parameter for a rate the protocol does not allow. */
    [[nodiscard]] Status align(uint32_t millihz, uint64_t blank);
    /* Main loop. Whether a frame posted on `head` is shown at a blank rather
     * than at once: a page is registered and the head is one of its. */
    bool                 latches(uint32_t head) const;
    /* Main loop. Frame `frame` of `head`, the image published under `key`, is
     * shown at the head's next blank. Nothing unless latches(head). */
    void                 post(uint32_t head, uint32_t frame, uint64_t key);
    /* Main loop, first thing in the show routine: a blank that latches from
     * here on queues the routine again, so no latch waits for one after it. */
    void                 showing();
    /* Main loop. A latched frame not yet shown on `head`: true, with its key,
     * once per latch. */
    bool                 take_unshown(uint32_t head, uint64_t &key);
    /* The thread's body, entered through AvDisplayClockRun. */
    void                 run();
    /* One pass at `now`: every blank due counted, latched, published and,
     * where the guest asked, raised; `next` is the earliest blank still to
     * come. False, doing nothing, once the clock is stopping. */
    bool                 tick(uint64_t now, uint64_t &next);

    bool invariants_hold() const;

private:
    /* Head `head`'s blanks at `now`, under the lock: whether it wants the
     * display line raised. */
    bool pass(uint32_t head, uint64_t now);
    void stop_thread();

    Host            &host_;
    FenceInterrupts &interrupts_;
    HostRoutine      show_;
    void            *show_context_;
    void            *lock_;
    void            *wake_;
    void            *thread_;
    bool             stopping_;
    /* Set when the show routine is queued, cleared as it starts. */
    bool             show_queued_;
    ClockPageWindow  page_;
    VblankSchedule   schedules_[heads];
    FrameLatch       latches_[heads];

    friend struct DisplayClockProbe;
};

}  // namespace av

#endif  // AV_DISPLAY_CLOCK_HPP
