/* The OpenGL present thread: what puts the frames swaps hand over on their
 * windows, so that a swap only queues its frame's read-back and returns.
 *
 * A swap used to read its frame back and blit it itself, and the read-back is
 * a round trip that waits for the host to run everything the frame queued: the
 * application's render thread did nothing while the host drew, and the host
 * nothing while the thread built the next frame. Now the swap sends the read
 * one-way into a slot of its presenter's room and hands the frame here with
 * the batch it went out in; this thread waits for that batch, blits the slot
 * onto the window and gives the slot back. A frame shows as soon as the host
 * has it - no later than before - and the render thread is already building
 * the next one.
 *
 * One per process, over one thread. It waits through a Kmt of its own, since a
 * Kmt's waits share its event; it sleeps on `handed` when nothing is queued,
 * and a swap waiting for a slot sleeps on `shown`. Every table below is shared
 * with the swaps' threads and read and written under `lock`, never across a
 * wait or a blit. A frame is shown in the order it was handed. */
#ifndef AV_GLICD_GL_PRESENT_LOOP_HPP
#define AV_GLICD_GL_PRESENT_LOOP_HPP

#include "icd/common/once_latch.hpp"
#include "icd/common/service_thread.hpp"
#include "icd/opengl/handed_frame.hpp"

#include <stdint.h>

namespace av {
class LogSink;
}  // namespace av

namespace av {
class Kmt;
class StateWriter;
}  // namespace av

namespace av::icd {
class ThreadLock;
class WakeSignal;
class WindowChains;
class WindowSurface;
}  // namespace av::icd

namespace av::glicd {

class BackBufferPresenter;
struct GlPresentLoopProbe;

class GlPresentLoop final : public icd::ServiceLoop {
public:
    static constexpr uint32_t max_frames = 32u;
    /* An idle thread looks again this often; a hand-over wakes it at once. */
    static constexpr uint32_t idle_milliseconds = 1000u;
    /* A swap waiting for a slot looks again this often, this many times,
     * before it gives the frame up: two seconds is dxgkrnl's patience with a
     * GPU, and a thread showing nothing for that long is not coming back. */
    static constexpr uint32_t slot_milliseconds = 4u;
    static constexpr uint32_t slot_looks = 500u;

    GlPresentLoop(icd::ServiceThread &thread, icd::WakeSignal &handed, icd::WakeSignal &shown,
                  icd::ThreadLock &lock, Kmt &kmt, icd::WindowSurface &surface, icd::WindowChains &chains,
                  LogSink &log);
    ~GlPresentLoop();
    GlPresentLoop(const GlPresentLoop &) = delete;
    GlPresentLoop &operator=(const GlPresentLoop &) = delete;

    /* Whether swaps may hand frames over: the thread runs, started here the
     * first time. A thread that would not start, or a loop refused, never is. */
    bool     ready();
    /* The shim could not make what the thread needs: swaps show their own
     * frames. */
    void     refuse();
    /* A slot of `owner`'s `slots` that no handed frame holds and that is not
     * `reserved`, waiting for the thread to show one while all are taken;
     * `slots` when none came free in time, and the caller drops its frame. */
    uint32_t free_slot(const BackBufferPresenter &owner, uint32_t slots, uint32_t reserved);
    /* Queues `frame` and wakes the thread; false when the table is full. */
    bool     hand(const HandedFrame &frame);
    /* Returns once nothing of `owner`'s is queued or being shown: before its
     * room or its pixels change, and before it goes. What the thread will not
     * show - it stopped, or showed nothing for slot_looks looks - is dropped. */
    void     settle(const BackBufferPresenter &owner);
    /* The windows' chains a presenter opens its slots in, and this thread
     * presents a frame drawn into a slot through. */
    icd::WindowChains &chains();
    /* For a reader of what the thread writes - a presenter's front buffer. */
    void     acquire();
    void     release();

    /* One frame: the oldest queued, waited for, blitted and given back; or,
     * with none queued, a sleep until one is. False once stopping. */
    bool turn() override;

    uint32_t queued() const;
    uint64_t shown_frames() const;
    uint64_t dropped_frames() const;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    enum class Stage : uint8_t { free, queued, showing };

    /* The queued frame handed first, or max_frames. */
    uint32_t oldest() const;
    uint32_t free_row() const;
    uint32_t held_by(const BackBufferPresenter &owner) const;
    void     drop_queued(const BackBufferPresenter &owner);
    bool     holds(const BackBufferPresenter &owner, uint32_t slot) const;
    /* Waits for the frame's batch, again while the kernel's bounded wait
     * times out on a host that is behind: false for a context that is gone,
     * or once stopping. */
    bool     wait_for(const BatchMark &mark);
    /* Shows the frame taken from `row` when its batch is done: a slot of its
     * window's chain presented, or its pixels blitted. */
    void     show(uint32_t row, const HandedFrame &frame);
    bool     stopping() const;
    void     log_refusal();

    icd::ServiceThread &thread_;
    icd::WakeSignal    &handed_;
    icd::WakeSignal    &shown_;
    icd::ThreadLock    &lock_;
    Kmt                &kmt_;
    icd::WindowSurface &surface_;
    icd::WindowChains  &chains_;
    LogSink       &log_;
    HandedFrame         frames_[max_frames];
    Stage               stages_[max_frames];
    uint64_t            orders_[max_frames];
    uint64_t            next_order_;
    uint64_t            shown_frames_;
    uint64_t            dropped_frames_;
    /* Read by the thread, written by the destructor on another. */
    uint32_t            stopping_;
    bool                refused_;
    icd::OnceLatch      refusal_logged_;

    friend struct GlPresentLoopProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_GL_PRESENT_LOOP_HPP
