#include "core/log/keyed_log.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/counted_wake_signal.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_thread.hpp"
#include "harness/icd/scripted_window_chains.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/common/frame_wait.hpp"
#include "icd/common/window_present_queue.hpp"

#include <cstdint>

using av::icd::FrameWait;

namespace {

struct Mark {
    std::uint32_t id = 0u;
};

class Readiness {
public:
    using Mark = ::Mark;

    FrameWait wait(const Mark &mark)
    {
        waited_ = mark.id;
        return answer_;
    }

    void          answer(FrameWait answer) { answer_ = answer; }
    std::uint32_t waited() const { return waited_; }

private:
    FrameWait     answer_ = FrameWait::drawn;
    std::uint32_t waited_ = 0u;
};

class Owner;

using Queue = av::icd::WindowPresentQueue<Owner, Readiness>;
using Frame = Queue::Frame;

class Owner {
public:
    void done(const Frame &frame, bool drawn)
    {
        finished_++;
        last_slot_ = frame.slot();
        last_drawn_ = drawn;
    }

    std::uint32_t finished() const { return finished_; }
    std::uint32_t last_slot() const { return last_slot_; }
    bool          last_drawn() const { return last_drawn_; }

private:
    std::uint32_t finished_ = 0u;
    std::uint32_t last_slot_ = 0u;
    bool          last_drawn_ = false;
};

struct Rig {
    av::MockPlatform          platform;
    av::RecordingLog          log;
    av::KeyedLog              lines{platform, log, "avtest: "};
    av::ScriptedThread        thread;
    av::CountedWakeSignal     handed;
    av::CountedWakeSignal     shown;
    av::LockTrace             trace;
    av::RecordingThreadLock   lock{trace, 'P'};
    av::ScriptedWindowSurface surface;
    av::ScriptedWindowChains  chains;
    Readiness                 readiness;
    Owner                     owner;
    Queue                     queue{platform, thread, handed, shown, lock, readiness, surface, chains, lines};
    int                       dc = 0;
    int                       window = 0;
    std::uint8_t              pixels[4 * 2 * 4] = {};

    Rig() { surface.add_dc(&dc, &window, 4u, 2u); }

    Frame frame(std::uint32_t slot, std::uint32_t mark = 1u)
    {
        return Frame(owner, slot, pixels, Frame::Rows::bgra_bottom_up, 4u, 2u, &window, Mark{mark});
    }
};

}

TEST(window_present_queue, a_handed_frame_is_shown_by_the_thread_and_its_owner_told_it_was_drawn)
{ Rig rig;
  CHECK(rig.queue.start());
  CHECK(rig.queue.hand(rig.frame(2u, 9u)));
  CHECK_EQ(rig.queue.queued(), 1u);
  CHECK_EQ(rig.handed.raises(), 1u);
  CHECK(rig.queue.invariants_hold());
  CHECK(rig.thread.step());
  CHECK_EQ(rig.readiness.waited(), 9u);
  CHECK_EQ(rig.surface.blits(), 1u);
  CHECK_EQ(rig.owner.finished(), 1u);
  CHECK_EQ(rig.owner.last_slot(), 2u);
  CHECK(rig.owner.last_drawn());
  CHECK_EQ(rig.queue.shown_frames(), 1u);
  CHECK_EQ(rig.queue.queued(), 0u);
  CHECK_EQ(rig.shown.raises(), 1u);
  CHECK(rig.queue.invariants_hold()); }

TEST(window_present_queue, a_frame_whose_batch_is_gone_is_dropped_and_its_owner_told_it_was_not_drawn)
{ Rig rig;
  CHECK(rig.queue.start());
  rig.readiness.answer(FrameWait::gone);
  CHECK(rig.queue.hand(rig.frame(1u)));
  CHECK(rig.thread.step());
  CHECK_EQ(rig.surface.blits(), 0u);
  CHECK_EQ(rig.owner.finished(), 1u);
  CHECK(!rig.owner.last_drawn());
  CHECK_EQ(rig.queue.shown_frames(), 0u);
  CHECK_EQ(rig.queue.dropped_frames(), 1u);
  CHECK(rig.queue.invariants_hold()); }

TEST(window_present_queue, showing_now_puts_the_frame_on_its_window_without_the_thread_or_the_counts)
{ Rig rig;
  CHECK(rig.queue.show_now(rig.frame(0u)));
  CHECK_EQ(rig.surface.blits(), 1u);
  CHECK(rig.surface.last_blit_dc() == &rig.dc);
  CHECK_EQ(rig.thread.starts(), 0u);
  CHECK_EQ(rig.queue.queued(), 0u);
  CHECK_EQ(rig.queue.shown_frames(), 0u);
  CHECK_EQ(rig.owner.finished(), 0u);
  rig.surface.refuse_next_blit();
  CHECK(!rig.queue.show_now(rig.frame(0u)));
  CHECK(rig.queue.show_now(rig.frame(0u)));
  CHECK(rig.queue.invariants_hold()); }

TEST(window_present_queue, a_frame_a_chain_holds_is_presented_by_its_slot_when_shown_now)
{ Rig rig;
  rig.chains.refuse_opens(false);
  std::uint64_t ids[2] = {}, sizes[2] = {};
  CHECK(rig.chains.open(&rig.window, 1u, 4u, 2u, av::icd::SlotLayout::rgba_bottom_up, 2u, ids, sizes));
  const Frame chained(rig.owner, 1u, nullptr, Frame::Rows::bgra_bottom_up, 4u, 2u, &rig.window, Mark{});
  CHECK(rig.queue.show_now(chained));
  CHECK_EQ(rig.chains.presents(), 1u);
  CHECK_EQ(rig.chains.last_slot(), 1u);
  CHECK_EQ(rig.surface.blits(), 0u); }

TEST(window_present_queue, waiting_for_a_shown_frame_waits_on_the_shown_signal_for_the_time_given)
{ Rig rig;
  rig.queue.await_shown(7u);
  CHECK_EQ(rig.shown.waits(), 1u);
  CHECK_EQ(rig.shown.last_wait(), 7u);
  CHECK_EQ(rig.handed.waits(), 0u);
  rig.queue.await_shown(0u);
  CHECK_EQ(rig.shown.waits(), 2u);
  CHECK_EQ(rig.shown.last_wait(), 0u); }
