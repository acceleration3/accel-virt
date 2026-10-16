/* Shows handed frames on their windows in the order they were handed: the
 * present thread of a process's OpenGL swaps and of its Vulkan presents, so
 * that a present only queues its frame's read-back or copy and returns.
 *
 * A present used to read its frame back and blit it itself, and the read-back
 * waits for the host to run everything the frame queued: the application's
 * render thread did nothing while the host drew, and the host nothing while
 * the thread built the next frame. Now a present sends the read or the copy
 * one-way into a free slot of its `Source` (a window's back buffer, a
 * swapchain) and hands the frame here; this thread waits for it to be drawn,
 * puts it on the window - a slot of the window's chain presented, or its
 * pixels blitted - and gives the slot back. A frame shows as soon as the host
 * has it, and the render thread is already building the next one.
 *
 * Two policies, each with two implementations: `Source` is the presenter
 * frames belong to (OpenGL's BackBufferPresenter, Vulkan's Swapchain), told
 * under the lock when the queue is done with one of its frames
 * (`done(frame, drawn)`); `Readiness` is how a frame is waited for (OpenGL's
 * BatchReady, a batch through a Kmt only this thread waits on; Vulkan's
 * FenceReady, the copy's fence), with its `Mark` the frame carries and
 * `wait(mark)` answering a FrameWait.
 *
 * One per process and API, over one thread. It sleeps on `handed` when
 * nothing is queued, and a presenter waiting for a slot sleeps on `shown`.
 * Every table below is shared with the presenters' threads and read and
 * written under `lock`, never across a wait or a blit. */
#ifndef AV_ICD_WINDOW_PRESENT_QUEUE_HPP
#define AV_ICD_WINDOW_PRESENT_QUEUE_HPP

#include "core/log/driver_log.hpp"
#include "core/platform/block_source.hpp"
#include "core/platform/thread_lock.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/support/growable_array.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/common/frame_wait.hpp"
#include "icd/common/handed_frame.hpp"
#include "icd/common/once_latch.hpp"
#include "icd/common/service_thread.hpp"
#include "icd/common/thread_wake.hpp"
#include "icd/common/window_chains.hpp"
#include "icd/common/window_surface.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::icd {

template <typename Source, typename Readiness>
class WindowPresentQueue final : public ServiceLoop {
public:
    using Mark  = typename Readiness::Mark;
    using Frame = HandedFrame<Source, Mark>;

    /* An idle thread looks again this often; a hand-over wakes it at once. */
    static constexpr uint32_t idle_milliseconds = 1000u;
    /* A presenter waiting for a slot looks again this often, this many times,
     * before it gives the frame up: two seconds is dxgkrnl's patience with a
     * GPU, and a thread showing nothing for that long is not coming back. */
    static constexpr uint32_t slot_milliseconds = 4u;
    static constexpr uint32_t slot_looks = 500u;

    /* `log` is the presenting driver's. */
    WindowPresentQueue(BlockSource &memory, ServiceThread &thread, ThreadWake &handed, ThreadWake &shown,
                       ThreadLock &lock, Readiness &readiness, WindowSurface &surface, WindowChains &chains,
                       DriverLog &log);
    ~WindowPresentQueue();
    WindowPresentQueue(const WindowPresentQueue &) = delete;
    WindowPresentQueue &operator=(const WindowPresentQueue &) = delete;

    /* Whether frames may be handed over: the thread runs, started here the
     * first time. A thread that would not start, or a queue refused, never is. */
    bool     start();
    /* The shim could not make what the thread needs: presenters show their
     * own frames. */
    void     refuse();
    /* A slot of `owner`'s `slots` that no handed frame holds and that is not
     * `reserved`, waiting for the thread to show one while all are taken;
     * `slots` when none came free in time, and the caller drops its frame. */
    uint32_t free_slot(const Source &owner, uint32_t slots, uint32_t reserved);
    /* Queues `frame` and wakes the thread; false, the frame dropped, when
     * there is no memory for a row. */
    bool     hand(const Frame &frame);
    /* Returns once nothing of `owner`'s is queued or being shown: before its
     * slots or its window change, and before it goes. What the thread will
     * not show - it stopped, or showed nothing for slot_looks looks - is
     * dropped, and its source told so. */
    void     settle(const Source &owner);
    /* Sleeps until the thread has shown a frame or `milliseconds` pass. */
    void     await_shown(uint32_t milliseconds);
    /* A frame drawn already put on its window on the calling thread, under
     * the lock: a presenter's own present when there is no thread to hand
     * it to. False when the window or its chain refused it. */
    bool     show_now(const Frame &frame);
    /* The windows' chains a presenter opens its slots in, and this thread
     * presents a frame drawn into a slot through. */
    WindowChains &chains();
    /* For a reader or writer of what the thread's done() writes. */
    void     acquire();
    void     release();

    /* One frame: the oldest queued, waited for, shown and given back; or,
     * with none queued, a sleep until one is. False once stopping. */
    bool turn() override;

    uint32_t queued() const;
    uint64_t shown_frames() const;
    uint64_t dropped_frames() const;

    void report(StateWriter &writer) const;

    /* A row holds a frame exactly while it is queued or showing, a held
     * frame's order is one handed out, and no two held frames share an
     * order. */
    bool invariants_hold() const;

private:
    enum class Stage : uint8_t { free, queued, showing };

    struct Row {
        Frame    frame_;
        Stage    stage_;
        uint64_t order_;
    };

    static constexpr uint32_t no_row = 0xFFFFFFFFu;
    static constexpr uint32_t pixel_bytes = 4u;

    /* The queued frame handed first, or no_row. */
    uint32_t oldest() const;
    uint32_t free_row() const;
    uint32_t held_by(const Source &owner) const;
    void     drop_queued(const Source &owner);
    bool     holds(const Source &owner, uint32_t slot) const;
    /* Waits for the frame to be drawn, again while the wait is bounded and
     * the frame is not drawn yet: false for one that never will be, or once
     * stopping. */
    bool     wait_for(const Mark &mark);
    /* Shows the frame taken from `row` once it is drawn. */
    void     show(uint32_t row, const Frame &frame);
    /* The frame on its window: false when the window or its chain refused it. */
    bool     put_on_window(const Frame &frame);
    /* An RGBA frame's rows with red and blue traded, in the scratch: a DIB is
     * BGRA. Null with no memory for them. A pixel at a time: a byte at a time
     * over 14 MB a frame was a fifth of a game's render thread. */
    const uint8_t *as_bgra(const Frame &frame);
    bool     stopping() const;
    void     log_refusal();

    BlockSource    &memory_;
    ServiceThread  &thread_;
    ThreadWake     &handed_;
    ThreadWake     &shown_;
    ThreadLock     &lock_;
    Readiness      &readiness_;
    WindowSurface  &surface_;
    WindowChains   &chains_;
    DriverLog      &lines_;
    /* As many as frames are ever handed at once - two for each window
     * presenting - and reused once free. */
    GrowableArray<Row> rows_;
    uint64_t        next_order_;
    uint64_t        shown_frames_;
    uint64_t        dropped_frames_;
    /* The thread's own: an RGBA frame traded into BGRA. */
    uint8_t        *scratch_;
    size_t          scratch_bytes_;
    /* Read by the thread, written by the destructor on another. */
    uint32_t        stopping_;
    bool            refused_;
    OnceLatch       refusal_logged_;
};

template <typename Source, typename Readiness>
WindowPresentQueue<Source, Readiness>::WindowPresentQueue(BlockSource &memory, ServiceThread &thread,
                                                          ThreadWake &handed, ThreadWake &shown, ThreadLock &lock,
                                                          Readiness &readiness, WindowSurface &surface,
                                                          WindowChains &chains, DriverLog &log)
    : memory_(memory), thread_(thread), handed_(handed), shown_(shown), lock_(lock), readiness_(readiness),
      surface_(surface), chains_(chains), lines_(log), rows_(memory, Pool::paged), next_order_(1u),
      shown_frames_(0u), dropped_frames_(0u), scratch_(nullptr), scratch_bytes_(0u), stopping_(0u), refused_(false),
      refusal_logged_()
{
    AV_SCOPE("WindowPresentQueue::WindowPresentQueue");
}

/* A turn asleep on `handed` wakes to the flag; one waiting for a frame wakes
 * when its bounded wait comes back. */
template <typename Source, typename Readiness>
WindowPresentQueue<Source, Readiness>::~WindowPresentQueue()
{
    AV_SCOPE("WindowPresentQueue::~WindowPresentQueue");
    __atomic_store_n(&stopping_, 1u, __ATOMIC_RELEASE);
    handed_.raise();
    thread_.stop();
    if (scratch_ != nullptr) memory_.release(scratch_);
}

template <typename Source, typename Readiness>
bool WindowPresentQueue<Source, Readiness>::start()
{
    AV_SCOPE("WindowPresentQueue::start");
    const ThreadLockHold held(lock_);
    if (!thread_.running() && !refused_ && thread_.start(*this) != Status::ok) refused_ = true;
    return thread_.running();
}

template <typename Source, typename Readiness>
void WindowPresentQueue<Source, Readiness>::refuse()
{
    AV_SCOPE("WindowPresentQueue::refuse");
    const ThreadLockHold held(lock_);
    refused_ = true;
}

template <typename Source, typename Readiness>
uint32_t WindowPresentQueue<Source, Readiness>::free_slot(const Source &owner, uint32_t slots, uint32_t reserved)
{
    AV_SCOPE("WindowPresentQueue::free_slot");
    for (uint32_t look = 0u; look < slot_looks; look++) {
        uint32_t slot = 0u;
        {
            const ThreadLockHold held(lock_);
            while (slot < slots && (slot == reserved || holds(owner, slot))) slot++;
        }
        if (slot != slots) return slot;
        static_cast<void>(shown_.wait(slot_milliseconds));
    }
    const ThreadLockHold held(lock_);
    dropped_frames_++;
    return slots;
}

template <typename Source, typename Readiness>
bool WindowPresentQueue<Source, Readiness>::hand(const Frame &frame)
{
    AV_SCOPE("WindowPresentQueue::hand");
    bool kept = false;
    {
        const ThreadLockHold held(lock_);
        const uint32_t       row = free_row();
        const Row            queued{frame, Stage::queued, next_order_};
        kept = row != no_row ? (rows_.at(row) = queued, true) : rows_.append(queued) == Status::ok;
        if (kept)
            next_order_++;
        else
            dropped_frames_++;
    }
    if (!kept) return false;
    handed_.raise();
    return true;
}

/* A frame being shown is waited for whatever happens, since the thread is
 * reading the slot it is in - and its wait is bounded. */
template <typename Source, typename Readiness>
void WindowPresentQueue<Source, Readiness>::settle(const Source &owner)
{
    AV_SCOPE("WindowPresentQueue::settle");
    for (uint32_t look = 0u;; look++) {
        uint32_t held_frames = 0u;
        {
            const ThreadLockHold held(lock_);
            if (look >= slot_looks || !thread_.running()) drop_queued(owner);
            held_frames = held_by(owner);
        }
        if (held_frames == 0u) return;
        static_cast<void>(shown_.wait(slot_milliseconds));
    }
}

template <typename Source, typename Readiness>
void WindowPresentQueue<Source, Readiness>::await_shown(uint32_t milliseconds)
{
    AV_SCOPE("WindowPresentQueue::await_shown");
    static_cast<void>(shown_.wait(milliseconds));
}

template <typename Source, typename Readiness>
bool WindowPresentQueue<Source, Readiness>::show_now(const Frame &frame)
{
    AV_SCOPE("WindowPresentQueue::show_now");
    const ThreadLockHold held(lock_);
    return put_on_window(frame);
}

template <typename Source, typename Readiness>
WindowChains &WindowPresentQueue<Source, Readiness>::chains()
{
    AV_SCOPE("WindowPresentQueue::chains");
    return chains_;
}

template <typename Source, typename Readiness>
void WindowPresentQueue<Source, Readiness>::acquire()
{
    AV_SCOPE("WindowPresentQueue::acquire");
    lock_.acquire();
}

template <typename Source, typename Readiness>
void WindowPresentQueue<Source, Readiness>::release()
{
    AV_SCOPE("WindowPresentQueue::release");
    lock_.release();
}

template <typename Source, typename Readiness>
bool WindowPresentQueue<Source, Readiness>::turn()
{
    AV_SCOPE("WindowPresentQueue::turn");
    uint32_t row = no_row;
    Frame    frame;
    {
        const ThreadLockHold held(lock_);
        row = oldest();
        if (row != no_row) {
            rows_.at(row).stage_ = Stage::showing;
            frame = rows_.at(row).frame_;
        }
    }
    if (row != no_row) {
        show(row, frame);
        return true;
    }
    if (stopping()) return false;
    static_cast<void>(handed_.wait(idle_milliseconds));
    return true;
}

template <typename Source, typename Readiness>
uint32_t WindowPresentQueue<Source, Readiness>::queued() const
{
    AV_SCOPE("WindowPresentQueue::queued");
    uint32_t count = 0u;
    for (const Row &row : rows_) count += row.stage_ != Stage::free ? 1u : 0u;
    return count;
}

template <typename Source, typename Readiness>
uint64_t WindowPresentQueue<Source, Readiness>::shown_frames() const
{
    AV_SCOPE("WindowPresentQueue::shown_frames");
    return shown_frames_;
}

template <typename Source, typename Readiness>
uint64_t WindowPresentQueue<Source, Readiness>::dropped_frames() const
{
    AV_SCOPE("WindowPresentQueue::dropped_frames");
    return dropped_frames_;
}

template <typename Source, typename Readiness>
void WindowPresentQueue<Source, Readiness>::report(StateWriter &writer) const
{
    AV_SCOPE("WindowPresentQueue::report");
    writer.open("presents", 0u);
    writer.flag("running", thread_.running());
    writer.flag("refused", refused_);
    writer.number("queued", queued());
    writer.number("shown", shown_frames_);
    writer.number("dropped", dropped_frames_);
    writer.close();
}

template <typename Source, typename Readiness>
bool WindowPresentQueue<Source, Readiness>::invariants_hold() const
{
    AV_SCOPE("WindowPresentQueue::invariants_hold");
    for (uint32_t row = 0u; row < rows_.size(); row++) {
        const Row &at = rows_.at(row);
        const bool held = at.stage_ != Stage::free;
        if (held != (at.frame_.owner() != nullptr)) return false;
        if (held && (at.order_ == 0u || at.order_ >= next_order_)) return false;
        for (uint32_t earlier = 0u; earlier < row && held; earlier++)
            if (rows_.at(earlier).stage_ != Stage::free && rows_.at(earlier).order_ == at.order_) return false;
    }
    return rows_.invariants_hold() && (scratch_ != nullptr) == (scratch_bytes_ != 0u);
}

template <typename Source, typename Readiness>
uint32_t WindowPresentQueue<Source, Readiness>::oldest() const
{
    AV_SCOPE("WindowPresentQueue::oldest");
    uint32_t found = no_row;
    for (uint32_t row = 0u; row < rows_.size(); row++) {
        if (rows_.at(row).stage_ != Stage::queued) continue;
        if (found == no_row || rows_.at(row).order_ < rows_.at(found).order_) found = row;
    }
    return found;
}

template <typename Source, typename Readiness>
uint32_t WindowPresentQueue<Source, Readiness>::free_row() const
{
    AV_SCOPE("WindowPresentQueue::free_row");
    for (uint32_t row = 0u; row < rows_.size(); row++)
        if (rows_.at(row).stage_ == Stage::free) return row;
    return no_row;
}

template <typename Source, typename Readiness>
uint32_t WindowPresentQueue<Source, Readiness>::held_by(const Source &owner) const
{
    AV_SCOPE("WindowPresentQueue::held_by");
    uint32_t count = 0u;
    for (const Row &row : rows_) count += row.stage_ != Stage::free && row.frame_.owner() == &owner ? 1u : 0u;
    return count;
}

template <typename Source, typename Readiness>
void WindowPresentQueue<Source, Readiness>::drop_queued(const Source &owner)
{
    AV_SCOPE("WindowPresentQueue::drop_queued");
    for (Row &row : rows_) {
        if (row.stage_ != Stage::queued || row.frame_.owner() != &owner) continue;
        row.frame_.owner()->done(row.frame_, false);
        row.frame_ = Frame();
        row.stage_ = Stage::free;
        dropped_frames_++;
    }
}

template <typename Source, typename Readiness>
bool WindowPresentQueue<Source, Readiness>::holds(const Source &owner, uint32_t slot) const
{
    AV_SCOPE("WindowPresentQueue::holds");
    for (const Row &row : rows_)
        if (row.stage_ != Stage::free && row.frame_.owner() == &owner && row.frame_.slot() == slot) return true;
    return false;
}

template <typename Source, typename Readiness>
bool WindowPresentQueue<Source, Readiness>::wait_for(const Mark &mark)
{
    AV_SCOPE("WindowPresentQueue::wait_for");
    for (;;) {
        const FrameWait waited = readiness_.wait(mark);
        if (waited == FrameWait::drawn) return true;
        if (waited == FrameWait::gone || stopping()) return false;
    }
}

template <typename Source, typename Readiness>
void WindowPresentQueue<Source, Readiness>::show(uint32_t row, const Frame &frame)
{
    AV_SCOPE("WindowPresentQueue::show");
    const bool drawn = wait_for(frame.mark());
    const bool shown = drawn && put_on_window(frame);
    {
        const ThreadLockHold held(lock_);
        frame.owner()->done(frame, drawn);
        rows_.at(row).frame_ = Frame();
        rows_.at(row).stage_ = Stage::free;
        if (shown)
            shown_frames_++;
        else
            dropped_frames_++;
    }
    if (drawn && !shown) log_refusal();
    shown_.raise();
}

template <typename Source, typename Readiness>
bool WindowPresentQueue<Source, Readiness>::put_on_window(const Frame &frame)
{
    AV_SCOPE("WindowPresentQueue::put_on_window");
    if (frame.chained()) return chains_.present(frame.window(), frame.slot());
    switch (frame.rows()) {
    case Frame::Rows::bgra_bottom_up:
        return surface_.blit_window(frame.window(), frame.width(), frame.height(), frame.pixels());
    case Frame::Rows::bgra_top_down:
        return surface_.blit_window_top_down(frame.window(), frame.width(), frame.height(), frame.pixels());
    case Frame::Rows::rgba_top_down: break;
    }
    const uint8_t *const traded = as_bgra(frame);
    return traded != nullptr && surface_.blit_window_top_down(frame.window(), frame.width(), frame.height(), traded);
}

template <typename Source, typename Readiness>
const uint8_t *WindowPresentQueue<Source, Readiness>::as_bgra(const Frame &frame)
{
    AV_SCOPE("WindowPresentQueue::as_bgra");
    const size_t pixels = size_t{frame.width()} * frame.height();
    const size_t bytes = pixels * pixel_bytes;
    if (bytes > scratch_bytes_) {
        if (scratch_ != nullptr) memory_.release(scratch_);
        scratch_ = static_cast<uint8_t *>(memory_.allocate(bytes, Pool::paged));
        scratch_bytes_ = scratch_ != nullptr ? bytes : 0u;
    }
    if (scratch_ == nullptr) return nullptr;
    const uint8_t *const rows = frame.pixels();
    for (size_t at = 0u; at < pixels; at++) {
        uint32_t pixel = 0u;
        __builtin_memcpy(&pixel, rows + at * pixel_bytes, pixel_bytes);
        pixel = (pixel & 0xFF00FF00u) | ((pixel >> 16u) & 0xFFu) | ((pixel & 0xFFu) << 16u);
        __builtin_memcpy(scratch_ + at * pixel_bytes, &pixel, pixel_bytes);
    }
    return scratch_;
}

template <typename Source, typename Readiness>
bool WindowPresentQueue<Source, Readiness>::stopping() const
{
    AV_SCOPE("WindowPresentQueue::stopping");
    return __atomic_load_n(&stopping_, __ATOMIC_ACQUIRE) != 0u;
}

template <typename Source, typename Readiness>
void WindowPresentQueue<Source, Readiness>::log_refusal()
{
    AV_SCOPE("WindowPresentQueue::log_refusal");
    if (!refusal_logged_.first()) return;
    lines_.write("a window refused a frame the present thread showed; later ones are not logged");
}

}  // namespace av::icd

#endif  // AV_ICD_WINDOW_PRESENT_QUEUE_HPP
