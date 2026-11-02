/* Where a write into a fullscreen primary waits for the flip before it to be
 * shown (PresentOrder::await_flip_shown).
 *
 * The host writes a primary's frame into the very texture DWM composes from
 * under the primary's key (D3d9FlipShares), and that write rides the
 * transport, which dxgkrnl never orders against its flips; nor does the
 * runtime queue a wait for the buffer on the kernel context - it blocks in
 * its present instead, after the frame was written. Until a vertical blank
 * has passed since the last flip, and DWM has had a few milliseconds after it
 * to compose from the flipped buffer, the primary written next may still be
 * the one DWM composes: Half-Life 2 showed a frame two ahead, then the one
 * before it, a few times a second while its camera turned.
 *
 * The wait is at the write, just before the present that flips it, and not at
 * the batch after the last present: a game that waits for its own vsync is
 * past the blank by then and does not wait at all, where a wait ahead of its
 * drawing cost a refresh on every other frame. */
#ifndef AV_UM_FLIP_SHOWN_WAIT_HPP
#define AV_UM_FLIP_SHOWN_WAIT_HPP

#include "core/support/status.hpp"
#include "guest/kmt/beam_position.hpp"
#include "icd/common/once_latch.hpp"

#include <stdint.h>

namespace av {

class KeyedLog;

}  // namespace av

namespace av::d3d {

class D3dSession;

class FlipShownWait {
public:
    FlipShownWait(D3dSession &session, KeyedLog &lines);
    FlipShownWait(const FlipShownWait &) = delete;
    FlipShownWait &operator=(const FlipShownWait &) = delete;

    /* A primary of VidPn `source` was flipped just now. */
    void flipped(uint32_t source);
    /* Returns once the last flip has had a blank and DWM's time after it to
     * be composed, at once when no flip is outstanding. */
    void await();

private:
    [[nodiscard]] Status await_blank();
    uint64_t             milliseconds() const;

    D3dSession    &session_;
    KeyedLog      &lines_;
    uint32_t       source_;
    bool           pending_;
    BeamPosition   flipped_;
    uint64_t       flipped_at_;
    uint64_t       frequency_;
    icd::OnceLatch refused_;
};

}  // namespace av::d3d

#endif  // AV_UM_FLIP_SHOWN_WAIT_HPP
