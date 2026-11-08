#ifndef AV_ICD_WINDOW_PRESENT_QUEUE_HPP
#define AV_ICD_WINDOW_PRESENT_QUEUE_HPP

#include "core/log/driver_log.hpp"
#include "core/platform/block_source.hpp"
#include "core/platform/thread_lock.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/support/bgra_rows.hpp"
#include "core/support/growable_array.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/common/frame_wait.hpp"
#include "icd/common/held_flusher.hpp"
#include "icd/common/handed_frame.hpp"
#include "icd/common/once_latch.hpp"
#include "icd/common/service_thread.hpp"
#include "icd/common/thread_wake.hpp"
#include "icd/common/window_chains.hpp"
#include "icd/common/window_surface.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::icd {

struct WindowPresentQueueProbe;

template <typename Source, typename Readiness>
class WindowPresentQueue final : public ServiceLoop {
public:
    using Mark  = typename Readiness::Mark;
    using Frame = HandedFrame<Source, Mark>;

    static constexpr uint32_t idle_milliseconds = 1000u;
    static constexpr uint32_t slot_milliseconds = 4u;
    static constexpr uint32_t slot_looks = 500u;

    WindowPresentQueue(BlockSource &memory, ServiceThread &thread, ThreadWake &handed, ThreadWake &shown,
                       ThreadLock &lock, Readiness &readiness, WindowSurface &surface, WindowChains &chains,
                       DriverLog &log);
    ~WindowPresentQueue();
    WindowPresentQueue(const WindowPresentQueue &) = delete;
    WindowPresentQueue &operator=(const WindowPresentQueue &) = delete;

    bool     start();
    void     refuse();
    uint32_t free_slot(const Source &owner, uint32_t slots, uint32_t reserved);
    uint32_t try_slot(const Source &owner, uint32_t slots, uint32_t reserved);
    void     keep(HeldFlusher &owner);
    void     forget(HeldFlusher &owner);
    bool     hand(const Frame &frame);
    void     settle(const Source &owner);
    void     await_shown(uint32_t milliseconds);
    bool     show_now(const Frame &frame);
    WindowChains &chains();
    void     acquire();
    void     release();

    bool turn() override;

    uint32_t queued() const;
    uint64_t shown_frames() const;
    uint64_t dropped_frames() const;

    void report(StateWriter &writer) const;

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

    uint32_t oldest() const;
    uint32_t free_row() const;
    uint32_t held_by(const Source &owner) const;
    void     drop_queued(const Source &owner);
    bool     holds(const Source &owner, uint32_t slot) const;
    bool     wait_for(const Mark &mark);
    void     show(uint32_t row, const Frame &frame);
    bool     put_on_window(const Frame &frame);
    const uint8_t *as_bgra(const Frame &frame);
    bool     stopping() const;
    void     log_refusal();
    bool     flush_keepers(uint32_t &rest);
    uint32_t keeper_of(const HeldFlusher *owner) const;

    BlockSource    &memory_;
    ServiceThread  &thread_;
    ThreadWake     &handed_;
    ThreadWake     &shown_;
    ThreadLock     &lock_;
    Readiness      &readiness_;
    WindowSurface  &surface_;
    WindowChains   &chains_;
    DriverLog      &lines_;
    GrowableArray<Row> rows_;
    GrowableArray<HeldFlusher *> keepers_;
    HeldFlusher    *flushing_;
    uint64_t        next_order_;
    uint64_t        shown_frames_;
    uint64_t        dropped_frames_;
    uint8_t        *scratch_;
    size_t          scratch_bytes_;
    uint32_t        stopping_;
    bool            refused_;
    OnceLatch       refusal_logged_;

    friend struct WindowPresentQueueProbe;
};

template <typename Source, typename Readiness>
WindowPresentQueue<Source, Readiness>::WindowPresentQueue(BlockSource &memory, ServiceThread &thread,
                                                          ThreadWake &handed, ThreadWake &shown, ThreadLock &lock,
                                                          Readiness &readiness, WindowSurface &surface,
                                                          WindowChains &chains, DriverLog &log)
    : memory_(memory), thread_(thread), handed_(handed), shown_(shown), lock_(lock), readiness_(readiness),
      surface_(surface), chains_(chains), lines_(log), rows_(memory, Pool::paged), keepers_(memory, Pool::paged), flushing_(nullptr),
      next_order_(1u),
      shown_frames_(0u), dropped_frames_(0u), scratch_(nullptr), scratch_bytes_(0u), stopping_(0u), refused_(false),
      refusal_logged_()
{
    AV_SCOPE("WindowPresentQueue::WindowPresentQueue");
}

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
        const uint32_t slot = try_slot(owner, slots, reserved);
        if (slot != slots) return slot;
        static_cast<void>(shown_.wait(slot_milliseconds));
    }
    const ThreadLockHold held(lock_);
    dropped_frames_++;
    return slots;
}

template <typename Source, typename Readiness>
uint32_t WindowPresentQueue<Source, Readiness>::try_slot(const Source &owner, uint32_t slots, uint32_t reserved)
{
    AV_SCOPE("WindowPresentQueue::try_slot");
    uint32_t slot = 0u;
    const ThreadLockHold held(lock_);
    while (slot < slots && (slot == reserved || holds(owner, slot))) slot++;
    return slot;
}

template <typename Source, typename Readiness>
void WindowPresentQueue<Source, Readiness>::keep(HeldFlusher &owner)
{
    AV_SCOPE("WindowPresentQueue::keep");
    {
        const ThreadLockHold held(lock_);
        if (keeper_of(&owner) == no_row) static_cast<void>(keepers_.append(&owner));
    }
    handed_.raise();
}

template <typename Source, typename Readiness>
void WindowPresentQueue<Source, Readiness>::forget(HeldFlusher &owner)
{
    AV_SCOPE("WindowPresentQueue::forget");
    for (;;) {
        {
            const ThreadLockHold held(lock_);
            const uint32_t       at = keeper_of(&owner);
            if (at != no_row) keepers_.remove_swap(at);
            if (flushing_ != &owner || !thread_.running()) return;
        }
        static_cast<void>(shown_.wait(slot_milliseconds));
    }
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
    uint32_t rest = idle_milliseconds;
    if (flush_keepers(rest)) return true;
    static_cast<void>(handed_.wait(rest));
    return true;
}

template <typename Source, typename Readiness>
bool WindowPresentQueue<Source, Readiness>::flush_keepers(uint32_t &rest)
{
    AV_SCOPE("WindowPresentQueue::flush_keepers");
    bool flushed = false;
    for (uint32_t index = 0u;; index++) {
        HeldFlusher *owner = nullptr;
        {
            const ThreadLockHold held(lock_);
            if (index >= keepers_.size()) return flushed;
            owner = keepers_.at(index);
            flushing_ = owner;
        }
        const uint32_t later = owner->flush_held();
        {
            const ThreadLockHold held(lock_);
            flushing_ = nullptr;
            const uint32_t at = keeper_of(owner);
            if (later == 0u && at != no_row) {
                keepers_.remove_swap(at);
                index--;
                flushed = true;
            }
        }
        shown_.raise();
        if (later != 0u && later < rest) rest = later;
    }
}

template <typename Source, typename Readiness>
uint32_t WindowPresentQueue<Source, Readiness>::keeper_of(const HeldFlusher *owner) const
{
    AV_SCOPE("WindowPresentQueue::keeper_of");
    for (uint32_t at = 0u; at < keepers_.size(); at++)
        if (keepers_.at(at) == owner) return at;
    return no_row;
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
    return rows_.invariants_hold() && keepers_.invariants_hold() && (scratch_ != nullptr) == (scratch_bytes_ != 0u);
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
    if (frame.rows() == Frame::Rows::bgra_bottom_up)
        return surface_.blit_window(frame.window(), frame.width(), frame.height(), frame.pixels());
    if (frame.rows() == Frame::Rows::bgra_top_down)
        return surface_.blit_window_top_down(frame.window(), frame.width(), frame.height(), frame.pixels());
    const uint8_t *const traded = as_bgra(frame);
    return traded != nullptr && surface_.blit_window_top_down(frame.window(), frame.width(), frame.height(), traded);
}

template <typename Source, typename Readiness>
const uint8_t *WindowPresentQueue<Source, Readiness>::as_bgra(const Frame &frame)
{
    AV_SCOPE("WindowPresentQueue::as_bgra");
    const uint64_t pixels = uint64_t{frame.width()} * frame.height();
    uint64_t       whole = 0u;
    if (__builtin_mul_overflow(pixels, uint64_t{pixel_bytes}, &whole)) return nullptr;
    if constexpr (sizeof(size_t) < sizeof(uint64_t))
        if (whole > ~size_t{0}) return nullptr;
    const size_t bytes = static_cast<size_t>(whole);
    if (bytes > scratch_bytes_) {
        if (scratch_ != nullptr) memory_.release(scratch_);
        scratch_ = static_cast<uint8_t *>(memory_.allocate(bytes, Pool::paged));
        scratch_bytes_ = scratch_ != nullptr ? bytes : 0u;
    }
    if (scratch_ == nullptr) return nullptr;
    const uint64_t pitch = uint64_t{frame.width()} * pixel_bytes;
    const BgraRows from(frame.pixels(), bytes, pitch);
    BgraRows to(scratch_, scratch_bytes_, pitch);
    return to.copy_from(from, frame.width(), frame.height(), BgraRows::Channels::red_blue_swapped) == Status::ok
               ? scratch_ : nullptr;
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

}

#endif
