/* Paces one presenter's frames to the interval it asked for: a number of the
 * adapter's vertical blanks each present waits for once its frame is on the
 * window, or, at 0, no wait and only the frame each refresh shows (FramePacer
 * ::admits) - the rest would be read back and put on the window only to be
 * covered before it was seen.
 *
 * OpenGL asks with wglSwapIntervalEXT, Vulkan with its present mode (FIFO is
 * 1, IMMEDIATE is 0). WGL_EXT_swap_control leaves what an interval past 1
 * means to the implementation: here it is that many blanks. A negative
 * interval belongs to WGL_EXT_swap_control_tear, which no driver here
 * reports, so asking for one is refused and the interval already set stands.
 *
 * The pacer is bound once the presenter knows its adapter and device; until
 * then, and after unbind, nothing waits and every frame is admitted. */
#ifndef AV_ICD_PRESENT_PACING_HPP
#define AV_ICD_PRESENT_PACING_HPP

#include "core/support/object_storage.hpp"
#include "icd/common/frame_pacer.hpp"

#include <stdint.h>

namespace av {
class Kmt;
class LogSink;
}  // namespace av

namespace av::icd {

class PresentPacing {
public:
    /* Past this a presenter is not pacing, it is stalling: four blanks is 15
     * frames a second on a 60 Hz panel. */
    static constexpr uint32_t max_blanks = 4u;

    /* One blank a present, as both APIs default to. */
    PresentPacing();
    ~PresentPacing();
    PresentPacing(const PresentPacing &) = delete;
    PresentPacing &operator=(const PresentPacing &) = delete;

    /* The adapter whose blanks are waited for; a second bind is refused. */
    [[nodiscard]] Status bind(Kmt &kmt, LogSink &log, uint32_t adapter, uint32_t device);
    void                 unbind();

    /* False for an interval no reported extension defines, which leaves the
     * one set as it was. */
    [[nodiscard]] bool ask(int32_t blanks);
    uint32_t           blanks() const;
    /* Whether a frame presented now is one the screen shows: every one when
     * presents wait for blanks or nothing is bound. */
    bool               admits();
    /* The wait a present owes once its frame is on the window. */
    void               after_present();
    /* Blanks presents have waited for, which a report names. */
    uint64_t           waited() const;

    bool invariants_hold() const;

private:
    ObjectStorage<FramePacer> pacer_;
    uint32_t                  blanks_;
    uint64_t                  waited_;
};

}  // namespace av::icd

#endif  // AV_ICD_PRESENT_PACING_HPP
