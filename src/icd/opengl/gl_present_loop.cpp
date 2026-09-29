#include "icd/opengl/gl_present_loop.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/kmt/kmt.hpp"
#include "icd/common/log_sink.hpp"
#include "icd/common/thread_lock.hpp"
#include "icd/common/wake_signal.hpp"
#include "icd/common/window_surface.hpp"
#include "icd/opengl/back_buffer_presenter.hpp"

namespace av::glicd {

GlPresentLoop::GlPresentLoop(icd::ServiceThread &thread, icd::WakeSignal &handed, icd::WakeSignal &shown,
                             icd::ThreadLock &lock, Kmt &kmt, icd::WindowSurface &surface, icd::LogSink &log)
    : thread_(thread), handed_(handed), shown_(shown), lock_(lock), kmt_(kmt), surface_(surface), log_(log),
      frames_{}, stages_{}, orders_{}, next_order_(1u), shown_frames_(0u), dropped_frames_(0u), stopping_(0u),
      refused_(false), refusal_logged_(false)
{
    AV_SCOPE("GlPresentLoop::GlPresentLoop");
}

/* A turn asleep on `handed` wakes to the flag; one waiting for a batch wakes
 * when the kernel's bounded wait comes back. */
GlPresentLoop::~GlPresentLoop()
{
    AV_SCOPE("GlPresentLoop::~GlPresentLoop");
    __atomic_store_n(&stopping_, 1u, __ATOMIC_RELEASE);
    handed_.raise();
    thread_.stop();
}

bool GlPresentLoop::ready()
{
    AV_SCOPE("GlPresentLoop::ready");
    lock_.acquire();
    if (!thread_.running() && !refused_ && thread_.start(*this) != Status::ok) refused_ = true;
    const bool running = thread_.running();
    lock_.release();
    return running;
}

void GlPresentLoop::refuse()
{
    AV_SCOPE("GlPresentLoop::refuse");
    lock_.acquire();
    refused_ = true;
    lock_.release();
}

uint32_t GlPresentLoop::free_slot(const BackBufferPresenter &owner, uint32_t slots, uint32_t reserved)
{
    AV_SCOPE("GlPresentLoop::free_slot");
    for (uint32_t look = 0u; look < slot_looks; look++) {
        lock_.acquire();
        uint32_t slot = 0u;
        while (slot < slots && (slot == reserved || holds(owner, slot))) slot++;
        lock_.release();
        if (slot != slots) return slot;
        static_cast<void>(shown_.wait(slot_milliseconds));
    }
    lock_.acquire();
    dropped_frames_++;
    lock_.release();
    return slots;
}

bool GlPresentLoop::hand(const HandedFrame &frame)
{
    AV_SCOPE("GlPresentLoop::hand");
    lock_.acquire();
    const uint32_t row = free_row();
    if (row != max_frames) {
        frames_[row] = frame;
        stages_[row] = Stage::queued;
        orders_[row] = next_order_++;
    }
    lock_.release();
    if (row == max_frames) return false;
    handed_.raise();
    return true;
}

/* A thread that stopped, or showed nothing for as long as a swap waits for a
 * slot, will not show what is still queued: that is dropped. A frame being
 * shown is waited for whatever happens, since the thread is reading the room
 * it is in - and its wait is bounded. */
void GlPresentLoop::settle(const BackBufferPresenter &owner)
{
    AV_SCOPE("GlPresentLoop::settle");
    for (uint32_t look = 0u;; look++) {
        lock_.acquire();
        if (look >= slot_looks || !thread_.running()) drop_queued(owner);
        const uint32_t held = held_by(owner);
        lock_.release();
        if (held == 0u) return;
        static_cast<void>(shown_.wait(slot_milliseconds));
    }
}

void GlPresentLoop::acquire()
{
    AV_SCOPE("GlPresentLoop::acquire");
    lock_.acquire();
}

void GlPresentLoop::release()
{
    AV_SCOPE("GlPresentLoop::release");
    lock_.release();
}

bool GlPresentLoop::turn()
{
    AV_SCOPE("GlPresentLoop::turn");
    lock_.acquire();
    const uint32_t row = oldest();
    HandedFrame    frame;
    if (row != max_frames) {
        stages_[row] = Stage::showing;
        frame = frames_[row];
    }
    lock_.release();
    if (row != max_frames) {
        show(row, frame);
        return true;
    }
    if (stopping()) return false;
    static_cast<void>(handed_.wait(idle_milliseconds));
    return true;
}

uint32_t GlPresentLoop::queued() const
{
    AV_SCOPE("GlPresentLoop::queued");
    uint32_t count = 0u;
    for (const Stage stage : stages_) count += stage != Stage::free ? 1u : 0u;
    return count;
}

uint64_t GlPresentLoop::shown_frames() const
{
    AV_SCOPE("GlPresentLoop::shown_frames");
    return shown_frames_;
}

uint64_t GlPresentLoop::dropped_frames() const
{
    AV_SCOPE("GlPresentLoop::dropped_frames");
    return dropped_frames_;
}

void GlPresentLoop::report(StateWriter &writer) const
{
    AV_SCOPE("GlPresentLoop::report");
    writer.open("presents", 0u);
    writer.flag("running", thread_.running());
    writer.flag("refused", refused_);
    writer.number("queued", queued());
    writer.number("shown", shown_frames_);
    writer.number("dropped", dropped_frames_);
    writer.close();
}

/* A row holds a frame exactly while it is queued or showing, a held frame's
 * order is one handed out, and no two held frames share an order. */
bool GlPresentLoop::invariants_hold() const
{
    AV_SCOPE("GlPresentLoop::invariants_hold");
    for (uint32_t row = 0u; row < max_frames; row++) {
        const bool held = stages_[row] != Stage::free;
        if (held != (frames_[row].owner() != nullptr)) return false;
        if (held && (orders_[row] == 0u || orders_[row] >= next_order_)) return false;
        for (uint32_t earlier = 0u; earlier < row && held; earlier++)
            if (stages_[earlier] != Stage::free && orders_[earlier] == orders_[row]) return false;
    }
    return true;
}

uint32_t GlPresentLoop::oldest() const
{
    AV_SCOPE("GlPresentLoop::oldest");
    uint32_t found = max_frames;
    for (uint32_t row = 0u; row < max_frames; row++) {
        if (stages_[row] != Stage::queued) continue;
        if (found == max_frames || orders_[row] < orders_[found]) found = row;
    }
    return found;
}

uint32_t GlPresentLoop::free_row() const
{
    AV_SCOPE("GlPresentLoop::free_row");
    for (uint32_t row = 0u; row < max_frames; row++)
        if (stages_[row] == Stage::free) return row;
    return max_frames;
}

uint32_t GlPresentLoop::held_by(const BackBufferPresenter &owner) const
{
    AV_SCOPE("GlPresentLoop::held_by");
    uint32_t count = 0u;
    for (uint32_t row = 0u; row < max_frames; row++)
        count += stages_[row] != Stage::free && frames_[row].owner() == &owner ? 1u : 0u;
    return count;
}

void GlPresentLoop::drop_queued(const BackBufferPresenter &owner)
{
    AV_SCOPE("GlPresentLoop::drop_queued");
    for (uint32_t row = 0u; row < max_frames; row++) {
        if (stages_[row] != Stage::queued || frames_[row].owner() != &owner) continue;
        frames_[row] = HandedFrame();
        stages_[row] = Stage::free;
        dropped_frames_++;
    }
}

bool GlPresentLoop::holds(const BackBufferPresenter &owner, uint32_t slot) const
{
    AV_SCOPE("GlPresentLoop::holds");
    for (uint32_t row = 0u; row < max_frames; row++)
        if (stages_[row] != Stage::free && frames_[row].owner() == &owner && frames_[row].slot() == slot)
            return true;
    return false;
}

bool GlPresentLoop::wait_for(const BatchMark &mark)
{
    AV_SCOPE("GlPresentLoop::wait_for");
    for (;;) {
        if (mark.wait(kmt_) == Status::ok) return true;
        if (stopping() || kmt_.last_code() != BatchMark::wait_timed_out) return false;
    }
}

void GlPresentLoop::show(uint32_t row, const HandedFrame &frame)
{
    AV_SCOPE("GlPresentLoop::show");
    const bool ready = wait_for(frame.mark());
    const bool drawn =
        ready && surface_.blit_window(frame.window(), frame.width(), frame.height(), frame.pixels());
    lock_.acquire();
    if (ready) frame.owner()->shown(frame.pixels(), frame.width(), frame.height());
    frames_[row] = HandedFrame();
    stages_[row] = Stage::free;
    if (drawn)
        shown_frames_++;
    else
        dropped_frames_++;
    lock_.release();
    if (ready && !drawn) log_refusal();
    shown_.raise();
}

bool GlPresentLoop::stopping() const
{
    AV_SCOPE("GlPresentLoop::stopping");
    return __atomic_load_n(&stopping_, __ATOMIC_ACQUIRE) != 0u;
}

void GlPresentLoop::log_refusal()
{
    AV_SCOPE("GlPresentLoop::log_refusal");
    if (refusal_logged_) return;
    refusal_logged_ = true;
    log_.write("rvkgl: a window refused a frame the present thread blitted; later ones are not logged");
}

}  // namespace av::glicd
