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
    static constexpr uint32_t heads = 16u;

    DisplayClock(Host &host, FenceInterrupts &interrupts, HostRoutine show, void *show_context);
    ~DisplayClock();
    DisplayClock(const DisplayClock &) = delete;
    DisplayClock &operator=(const DisplayClock &) = delete;

    [[nodiscard]] Status initialize();
    [[nodiscard]] Status attach(uint64_t address);
    void                 detach();
    [[nodiscard]] Status retime(uint32_t head, uint32_t millihz);
    [[nodiscard]] Status retime_all(uint32_t millihz);
    uint32_t             refresh_microhz() const;
    [[nodiscard]] Status align(int64_t shift);
    bool                 latches(uint32_t head) const;
    void                 post(uint32_t head, uint32_t frame, uint64_t key);
    void                 showing();
    bool                 take_unshown(uint32_t head, uint64_t &key);
    void                 run();
    bool                 tick(uint64_t now, uint64_t &next);

    bool invariants_hold() const;

private:
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
    bool             show_queued_;
    ClockPageWindow  page_;
    VblankSchedule   schedules_[heads];
    FrameLatch       latches_[heads];

    friend struct DisplayClockProbe;
};

}

#endif
