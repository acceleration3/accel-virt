#include "icd/opengl/gl_present_loop.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/kmt/kmt.hpp"
#include "core/log/log_sink.hpp"
#include "icd/opengl/wake_signal.hpp"
#include "icd/common/window_chains.hpp"
#include "icd/common/window_surface.hpp"
#include "icd/opengl/back_buffer_presenter.hpp"

namespace av::glicd {

GlPresentLoop::GlPresentLoop(BlockSource &memory, icd::ServiceThread &thread, icd::WakeSignal &handed,
                             icd::WakeSignal &shown,
                             ThreadLock &lock, Kmt &kmt, icd::WindowSurface &surface,
                             icd::WindowChains &chains, LogSink &log)
    : thread_(thread), handed_(handed), shown_(shown), lock_(lock), kmt_(kmt), surface_(surface), chains_(chains),
      log_(log),
      rows_(memory, Pool::paged), next_order_(1u), shown_frames_(0u), dropped_frames_(0u), stopping_(0u),
      refused_(false), refusal_logged_()
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
    const Row      queued{frame, Stage::queued, next_order_};
    const bool     kept = row != no_row ? (rows_.at(row) = queued, true) : rows_.append(queued) == Status::ok;
    if (kept)
        next_order_++;
    else
        dropped_frames_++;
    lock_.release();
    if (!kept) return false;
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
    if (row != no_row) {
        rows_.at(row).stage_ = Stage::showing;
        frame = rows_.at(row).frame_;
    }
    lock_.release();
    if (row != no_row) {
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
    for (const Row &row : rows_) count += row.stage_ != Stage::free ? 1u : 0u;
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
    for (uint32_t row = 0u; row < rows_.size(); row++) {
        const Row &at = rows_.at(row);
        const bool held = at.stage_ != Stage::free;
        if (held != (at.frame_.owner() != nullptr)) return false;
        if (held && (at.order_ == 0u || at.order_ >= next_order_)) return false;
        for (uint32_t earlier = 0u; earlier < row && held; earlier++)
            if (rows_.at(earlier).stage_ != Stage::free && rows_.at(earlier).order_ == at.order_) return false;
    }
    return rows_.invariants_hold();
}

uint32_t GlPresentLoop::oldest() const
{
    AV_SCOPE("GlPresentLoop::oldest");
    uint32_t found = no_row;
    for (uint32_t row = 0u; row < rows_.size(); row++) {
        if (rows_.at(row).stage_ != Stage::queued) continue;
        if (found == no_row || rows_.at(row).order_ < rows_.at(found).order_) found = row;
    }
    return found;
}

uint32_t GlPresentLoop::free_row() const
{
    AV_SCOPE("GlPresentLoop::free_row");
    for (uint32_t row = 0u; row < rows_.size(); row++)
        if (rows_.at(row).stage_ == Stage::free) return row;
    return no_row;
}

uint32_t GlPresentLoop::held_by(const BackBufferPresenter &owner) const
{
    AV_SCOPE("GlPresentLoop::held_by");
    uint32_t count = 0u;
    for (const Row &row : rows_) count += row.stage_ != Stage::free && row.frame_.owner() == &owner ? 1u : 0u;
    return count;
}

void GlPresentLoop::drop_queued(const BackBufferPresenter &owner)
{
    AV_SCOPE("GlPresentLoop::drop_queued");
    for (Row &row : rows_) {
        if (row.stage_ != Stage::queued || row.frame_.owner() != &owner) continue;
        row.frame_ = HandedFrame();
        row.stage_ = Stage::free;
        dropped_frames_++;
    }
}

bool GlPresentLoop::holds(const BackBufferPresenter &owner, uint32_t slot) const
{
    AV_SCOPE("GlPresentLoop::holds");
    for (const Row &row : rows_)
        if (row.stage_ != Stage::free && row.frame_.owner() == &owner && row.frame_.slot() == slot) return true;
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
    const bool chained = frame.chained();
    const bool drawn = ready && (chained ? chains_.present(frame.window(), frame.slot())
                                         : surface_.blit_window(frame.window(), frame.width(), frame.height(),
                                                                frame.pixels()));
    lock_.acquire();
    if (ready && !chained) frame.owner()->shown(frame.pixels(), frame.width(), frame.height());
    rows_.at(row).frame_ = HandedFrame();
    rows_.at(row).stage_ = Stage::free;
    if (drawn)
        shown_frames_++;
    else
        dropped_frames_++;
    lock_.release();
    if (ready && !drawn) log_refusal();
    shown_.raise();
}

icd::WindowChains &GlPresentLoop::chains()
{
    AV_SCOPE("GlPresentLoop::chains");
    return chains_;
}

bool GlPresentLoop::stopping() const
{
    AV_SCOPE("GlPresentLoop::stopping");
    return __atomic_load_n(&stopping_, __ATOMIC_ACQUIRE) != 0u;
}

void GlPresentLoop::log_refusal()
{
    AV_SCOPE("GlPresentLoop::log_refusal");
    if (!refusal_logged_.first()) return;
    log_.write("avgl: a window refused a frame the present thread blitted; later ones are not logged");
}

}  // namespace av::glicd
