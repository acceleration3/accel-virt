/* The vertical blanks the driver reports, counted over a window and judged
 * against the mode the adapter committed: DWM's frame pacing is built on them,
 * and a rate that drifts is a desktop that stutters.
 *
 * What is judged is the *shortest* interval between two blanks, and it is
 * judged against one to two frames, not against one. dxgkrnl signals the
 * vertical blank with KePulseEvent (ADAPTER_DISPLAY::SignalVerticalBlankEvent,
 * dxgkrnl.sys @14002e378), which wakes only the threads already waiting: a loop
 * that returns to user mode and waits again misses whatever pulses while it is
 * not there. On a real guest the shortest interval a waiter sees is 1.8 frames
 * of a driver reporting every 17.08 ms, so what a waiter can prove is that
 * blanks keep coming no slower than every other frame and never faster than
 * one. The driver's own rate is the runner's to check, from the line
 * SyntheticVsync logs (feature 038, US3). */
#ifndef AV_VSYNC_SCENARIO_HPP
#define AV_VSYNC_SCENARIO_HPP

#include "guest/suite/scenario_steps.hpp"

#include <stdint.h>

namespace av {

class AdapterSearch;
class GuestReport;
class GuestSession;
class Kmt;

class VsyncScenario {
public:
    /* The rate every mode is committed at when the device is not told another
     * (DisplayMode::default_refresh_millihz), and what a source that does not
     * report its mode is judged by. */
    static constexpr uint32_t expected_hz = 60u;
    static constexpr uint32_t tolerance_percent = 5u;
    /* Twenty seconds of blanks at the rate a pulse-woken loop catches: enough
     * that the shortest interval is one that was not missed, and well inside
     * the runner's step timeout. */
    static constexpr uint32_t counted_blanks = 600u;
    static constexpr uint32_t source = 0u;

    VsyncScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search);

    void run();

    /* Hundredths of a hertz of the shortest interval it saw: the rate the
     * driver keeps, where the average is the rate this process caught. Zero
     * until it has seen two blanks with the clock moving between them. */
    uint32_t rate_centihertz() const;
    /* Blanks the loop caught over the whole window, in hundredths of a hertz:
     * the measurement a missed pulse shows up in. */
    uint32_t caught_centihertz() const;
    /* Whether the shortest interval is between one committed frame and two,
     * within the tolerance: a blank that was not missed, and no blank missed
     * twice over. */
    bool     paced() const;

private:
    /* Waits out one blank to open the window, then counts. */
    [[nodiscard]] Status count(GuestSession &session);

    Kmt                 &kmt_;
    const AdapterSearch &search_;
    ScenarioSteps        steps_;
    uint32_t             blanks_;
    uint64_t             first_tick_;
    uint64_t             last_tick_;
    uint64_t             shortest_;
    uint64_t             frequency_;
    /* The rate the source is committed at, in hundredths of a hertz. */
    uint32_t             expected_centihertz_;
};

}  // namespace av

#endif  // AV_VSYNC_SCENARIO_HPP
