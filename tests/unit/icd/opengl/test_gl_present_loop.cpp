#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/transport/batch_mark.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/counted_wake_signal.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_thread.hpp"
#include "harness/icd/scripted_window_chains.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "icd/common/window_present_queue.hpp"
#include "icd/opengl/batch_ready.hpp"
#include "icd/opengl/back_buffer_presenter.hpp"
#include "icd/opengl/guest_gl_context.hpp"

#include "gl_dispatch_client.h"

#include <cstdint>
#include <cstring>

using av::BatchMark;
using av::GlContextOptions;
using av::MockKmt;
using av::Status;
using av::glicd::BackBufferPresenter;
using av::glicd::GlFrame;
using av::glicd::GlPresentQueue;
using av::glicd::GuestGlContext;

namespace av::icd {

struct WindowPresentQueueProbe {
    template <typename Queue>
    static void stop(Queue &queue) { queue.stopping_ = 1u; }
    template <typename Queue>
    static void set_order(Queue &queue, std::uint32_t row, std::uint64_t order) { queue.rows_.at(row).order_ = order; }
};

}

namespace {

using Probe = av::icd::WindowPresentQueueProbe;

struct Rig {
    av::GlContextRig          context_rig;
    GuestGlContext            context{context_rig.connection(), context_rig.platform(), context_rig.send_lock(),
                                      context_rig.groups(), context_rig.lines(), context_rig.extensions(),
                                      context_rig.handles(), 1, GlContextOptions()};
    av::ScriptedThread        thread;
    av::CountedWakeSignal     handed;
    av::CountedWakeSignal     shown;
    av::LockTrace             trace;
    av::RecordingThreadLock   lock{trace, 'P'};
    av::ScriptedWindowSurface surface;
    av::glicd::BatchReady     ready{context_rig.kmt()};
    av::ScriptedWindowChains  chains;
    GlPresentQueue            loop{context_rig.platform(), thread, handed, shown, lock, ready, surface, chains,
                                   context_rig.lines()};
    BackBufferPresenter       first{context_rig.platform(), context_rig.lines()};
    BackBufferPresenter       second{context_rig.platform(), context_rig.lines()};
    int                       dcs[2] = {};
    int                       windows[2] = {};
    std::uint8_t              pixels[4 * 2 * 4];

    Rig()
    {
        context_rig.host().gl_made("renderer", "4.6");
        CHECK_STATUS(context.make(), Status::ok);
        for (int index = 0; index < 2; index++) surface.add_dc(&dcs[index], &windows[index], 4u, 2u);
        for (std::size_t at = 0u; at < sizeof pixels; at++) pixels[at] = static_cast<std::uint8_t>(at + 1u);
    }
    BatchMark sent()
    {
        av::glproto::av_glFlush(*context.wire());
        context.flush();
        return context.mark();
    }
    GlFrame frame(BackBufferPresenter &owner, std::uint32_t slot, int window, const BatchMark &mark,
                  GlFrame::Rows rows = GlFrame::Rows::bgra_bottom_up)
    {
        return GlFrame(owner, slot, pixels, rows, 4u, 2u, &windows[window], mark);
    }
    std::uint32_t waits() { return context_rig.kmt().calls(MockKmt::Call::wait_for_context); }
};

}

TEST(gl_present_loop, a_frame_handed_over_is_shown_on_its_window_once_its_batch_is_done)
{ Rig rig;
  CHECK(rig.loop.invariants_hold());
  CHECK(rig.loop.start());
  CHECK(rig.loop.start());
  CHECK_EQ(rig.thread.starts(), 1u);
  const GlFrame frame = rig.frame(rig.first, 1u, 0, rig.sent());
  CHECK(frame.owner() == &rig.first);
  CHECK_EQ(frame.slot(), 1u);
  CHECK(frame.window() == &rig.windows[0]);
  CHECK(frame.mark().waitable());
  CHECK(rig.loop.hand(frame));
  CHECK_EQ(rig.handed.raises(), 1u);
  CHECK_EQ(rig.loop.queued(), 1u);
  CHECK(rig.loop.invariants_hold());
  const std::uint32_t waits = rig.waits();
  CHECK(rig.thread.step());
  CHECK_EQ(rig.waits() - waits, 1u);
  CHECK_EQ(rig.surface.blits(), 1u);
  CHECK(rig.surface.last_blit_dc() == &rig.dcs[0]);
  CHECK_EQ(rig.surface.last_blit_width(), 4u);
  CHECK_EQ(rig.surface.last_blit_height(), 2u);
  CHECK(std::memcmp(rig.surface.last_blit_bytes(), rig.pixels, sizeof rig.pixels) == 0);
  CHECK_EQ(rig.loop.shown_frames(), 1u);
  CHECK_EQ(rig.loop.dropped_frames(), 0u);
  CHECK_EQ(rig.loop.queued(), 0u);
  CHECK_EQ(rig.shown.raises(), 1u);
  CHECK(rig.loop.invariants_hold()); }

TEST(gl_present_loop, frames_are_shown_in_the_order_they_were_handed)
{ Rig rig;
  CHECK(rig.loop.start());
  const BatchMark mark = rig.sent();
  CHECK(rig.loop.hand(rig.frame(rig.first, 0u, 0, mark)));
  CHECK(rig.loop.hand(rig.frame(rig.second, 0u, 1, mark)));
  CHECK(rig.thread.step());
  CHECK(rig.surface.last_blit_dc() == &rig.dcs[0]);
  CHECK(rig.loop.hand(rig.frame(rig.first, 1u, 0, mark)));
  CHECK(rig.thread.step());
  CHECK(rig.surface.last_blit_dc() == &rig.dcs[1]);
  CHECK(rig.thread.step());
  CHECK(rig.surface.last_blit_dc() == &rig.dcs[0]);
  CHECK_EQ(rig.loop.shown_frames(), 3u); }

TEST(gl_present_loop, an_idle_turn_sleeps_on_the_hand_over_and_a_stopping_one_ends_the_thread)
{ Rig rig;
  CHECK(rig.loop.start());
  CHECK(rig.thread.step());
  CHECK_EQ(rig.handed.waits(), 1u);
  CHECK_EQ(rig.handed.last_wait(), GlPresentQueue::idle_milliseconds);
  CHECK(rig.loop.hand(rig.frame(rig.first, 0u, 0, rig.sent())));
  Probe::stop(rig.loop);
  CHECK(rig.thread.step());
  CHECK_EQ(rig.loop.shown_frames(), 1u);
  CHECK(!rig.thread.step());
  CHECK_EQ(rig.handed.waits(), 1u); }

TEST(gl_present_loop, a_thread_that_will_not_start_or_a_refused_loop_hands_nothing_over)
{ { Rig rig;
    rig.thread.refuse_start();
    CHECK(!rig.loop.start());
    CHECK(!rig.loop.start());
    CHECK_EQ(rig.thread.starts(), 0u);
    CHECK(!rig.thread.running()); }
  Rig rig;
  rig.loop.refuse();
  CHECK(!rig.loop.start());
  CHECK_EQ(rig.thread.starts(), 0u); }

TEST(gl_present_loop, a_free_slot_is_one_no_handed_frame_holds)
{ Rig rig;
  CHECK(rig.loop.start());
  constexpr std::uint32_t slots = BackBufferPresenter::slot_count;
  CHECK_EQ(rig.loop.free_slot(rig.first, slots, slots), 0u);
  CHECK(rig.loop.hand(rig.frame(rig.first, 0u, 0, rig.sent())));
  CHECK_EQ(rig.loop.free_slot(rig.first, slots, slots), 1u);
  CHECK_EQ(rig.loop.free_slot(rig.second, slots, slots), 0u);
  CHECK_EQ(rig.shown.waits(), 0u);
  CHECK_EQ(rig.loop.free_slot(rig.first, slots, 1u), slots);
  CHECK_EQ(rig.shown.waits(), GlPresentQueue::slot_looks);
  CHECK_EQ(rig.shown.last_wait(), GlPresentQueue::slot_milliseconds);
  CHECK_EQ(rig.loop.dropped_frames(), 1u);
  CHECK(rig.thread.step());
  CHECK_EQ(rig.loop.free_slot(rig.first, slots, 1u), 0u); }

TEST(gl_present_loop, the_table_grows_with_the_frames_handed_and_a_row_that_cannot_be_had_drops_the_frame)
{ Rig rig;
  CHECK(rig.loop.start());
  const BatchMark mark = rig.sent();
  constexpr std::uint32_t many = 200u;
  for (std::uint32_t row = 0u; row < many; row++) CHECK(rig.loop.hand(rig.frame(rig.first, 0u, 0, mark)));
  CHECK_EQ(rig.handed.raises(), many);
  CHECK_EQ(rig.loop.queued(), many);
  CHECK_EQ(rig.loop.dropped_frames(), 0u);
  CHECK(rig.loop.invariants_hold());
  for (std::uint32_t row = 0u; row < many; row++) CHECK(rig.thread.step());
  CHECK_EQ(rig.loop.shown_frames(), many);
  CHECK_EQ(rig.loop.queued(), 0u);
  Rig refused;
  refused.context_rig.platform().fail_allocation_at(1u);
  CHECK(!refused.loop.hand(refused.frame(refused.first, 0u, 0, BatchMark())));
  CHECK_EQ(refused.handed.raises(), 0u);
  CHECK_EQ(refused.loop.dropped_frames(), 1u);
  CHECK_EQ(refused.loop.queued(), 0u);
  CHECK(refused.loop.invariants_hold()); }

TEST(gl_present_loop, a_frame_is_put_on_its_window_by_the_rows_it_was_drawn_in)
{ Rig rig;
  CHECK(rig.loop.start());
  const BatchMark mark = rig.sent();
  CHECK(rig.loop.hand(rig.frame(rig.first, 0u, 0, mark, GlFrame::Rows::bgra_top_down)));
  CHECK(rig.thread.step());
  CHECK_EQ(rig.surface.blits(), 1u);
  CHECK(rig.surface.last_blit_top_down());
  CHECK(std::memcmp(rig.surface.last_blit_bytes(), rig.pixels, sizeof rig.pixels) == 0);
  CHECK(rig.loop.hand(rig.frame(rig.first, 0u, 0, mark, GlFrame::Rows::rgba_top_down)));
  CHECK(rig.thread.step());
  CHECK_EQ(rig.surface.blits(), 2u);
  CHECK(rig.surface.last_blit_top_down());
  for (std::size_t at = 0u; at < sizeof rig.pixels; at += 4u) {
    CHECK_EQ(rig.surface.last_blit_bytes()[at], rig.pixels[at + 2u]);
    CHECK_EQ(rig.surface.last_blit_bytes()[at + 1u], rig.pixels[at + 1u]);
    CHECK_EQ(rig.surface.last_blit_bytes()[at + 2u], rig.pixels[at]);
    CHECK_EQ(rig.surface.last_blit_bytes()[at + 3u], rig.pixels[at + 3u]);
  }
  CHECK_EQ(rig.loop.shown_frames(), 2u);
  CHECK(rig.loop.invariants_hold()); }

TEST(gl_present_loop, a_frame_the_chain_holds_is_presented_by_its_slot_not_blitted)
{ Rig rig;
  CHECK(rig.loop.start());
  const BatchMark mark = rig.sent();
  rig.chains.refuse_opens(false);
  std::uint64_t ids[2] = {}, sizes[2] = {};
  CHECK(rig.chains.open(&rig.windows[0], 1u, 4u, 2u, av::icd::SlotLayout::rgba_bottom_up, 2u, ids, sizes));
  GlFrame chained(rig.first, 1u, nullptr, GlFrame::Rows::bgra_bottom_up, 4u, 2u, &rig.windows[0], mark);
  CHECK(chained.chained());
  CHECK(rig.loop.hand(chained));
  CHECK(rig.thread.step());
  CHECK_EQ(rig.chains.presents(), 1u);
  CHECK_EQ(rig.chains.last_slot(), 1u);
  CHECK_EQ(rig.surface.blits(), 0u);
  CHECK_EQ(rig.loop.shown_frames(), 1u);
  rig.chains.refuse_presents(true);
  CHECK(rig.loop.hand(chained));
  CHECK(rig.thread.step());
  CHECK_EQ(rig.loop.dropped_frames(), 1u);
  CHECK(rig.loop.invariants_hold()); }

TEST(gl_present_loop, settling_waits_for_the_thread_and_drops_what_it_will_not_show)
{ { Rig rig;
    rig.loop.settle(rig.first);
    CHECK_EQ(rig.shown.waits(), 0u);
    CHECK(rig.loop.hand(rig.frame(rig.first, 0u, 0, rig.sent())));
    CHECK(rig.loop.hand(rig.frame(rig.second, 0u, 0, rig.sent())));
    rig.loop.settle(rig.first);
    CHECK_EQ(rig.shown.waits(), 0u);
    CHECK_EQ(rig.loop.dropped_frames(), 1u);
    CHECK_EQ(rig.loop.queued(), 1u); }
  Rig rig;
  CHECK(rig.loop.start());
  CHECK(rig.loop.hand(rig.frame(rig.first, 0u, 0, rig.sent())));
  rig.loop.settle(rig.first);
  CHECK_EQ(rig.shown.waits(), GlPresentQueue::slot_looks);
  CHECK_EQ(rig.loop.dropped_frames(), 1u);
  CHECK_EQ(rig.loop.queued(), 0u);
  CHECK_EQ(rig.surface.blits(), 0u); }

TEST(gl_present_loop, a_host_behind_is_waited_for_again_and_a_frame_that_cannot_be_is_dropped)
{ Rig rig;
  CHECK(rig.loop.start());
  CHECK(rig.loop.hand(rig.frame(rig.first, 0u, 0, rig.sent())));
  rig.context_rig.kmt().fail_first(MockKmt::Call::wait_for_context, 1u, BatchMark::wait_timed_out);
  std::uint32_t waits = rig.waits();
  CHECK(rig.thread.step());
  CHECK_EQ(rig.waits() - waits, 2u);
  CHECK_EQ(rig.loop.shown_frames(), 1u);
  CHECK(rig.loop.hand(rig.frame(rig.first, 0u, 0, BatchMark())));
  CHECK(rig.thread.step());
  CHECK_EQ(rig.loop.dropped_frames(), 1u);
  CHECK_EQ(rig.surface.blits(), 1u);
  CHECK(rig.loop.hand(rig.frame(rig.first, 0u, 0, rig.sent())));
  rig.context_rig.kmt().fail_first(MockKmt::Call::wait_for_context, 1u, 0xC0000001u);
  CHECK(rig.thread.step());
  CHECK_EQ(rig.loop.dropped_frames(), 2u);
  CHECK(rig.loop.hand(rig.frame(rig.first, 0u, 0, rig.sent())));
  rig.context_rig.kmt().fail_first(MockKmt::Call::wait_for_context, 1u, BatchMark::wait_timed_out);
  Probe::stop(rig.loop);
  waits = rig.waits();
  CHECK(rig.thread.step());
  CHECK_EQ(rig.waits() - waits, 1u);
  CHECK_EQ(rig.loop.dropped_frames(), 3u);
  CHECK_EQ(rig.surface.blits(), 1u);
  CHECK_EQ(rig.context_rig.log().lines(), 0u); }

TEST(gl_present_loop, a_window_that_refuses_a_frame_is_logged_once)
{ Rig rig;
  CHECK(rig.loop.start());
  for (int refused = 0; refused < 2; refused++) {
    CHECK(rig.loop.hand(rig.frame(rig.first, 0u, 0, rig.sent())));
    rig.surface.refuse_next_blit();
    CHECK(rig.thread.step());
  }
  CHECK_EQ(rig.loop.dropped_frames(), 2u);
  CHECK_EQ(rig.context_rig.log().lines(), 1u);
  CHECK_EQ(rig.context_rig.log().holding("a window refused a frame the present thread showed"), 1u); }

TEST(gl_present_loop, reports_its_thread_and_frames_and_lends_its_lock)
{ Rig rig;
  CHECK(rig.loop.start());
  CHECK(rig.loop.hand(rig.frame(rig.first, 0u, 0, rig.sent())));
  av::icd::ThreadWake &handed = rig.handed;
  handed.raise();
  rig.loop.acquire();
  rig.loop.release();
  char            text[128];
  av::StateWriter writer(text, sizeof text);
  rig.loop.report(writer);
  CHECK(std::strcmp(writer.dump(), "presents/0 running=true refused=false queued=1 shown=0 dropped=0") == 0); }

TEST(gl_present_loop_invariants, fail_for_a_row_held_empty_or_an_order_not_handed_out_or_twice)
{ { Rig rig;
    CHECK(rig.loop.hand(GlFrame()));
    CHECK(!rig.loop.invariants_hold()); }
  { Rig rig;
    CHECK(rig.loop.hand(rig.frame(rig.first, 0u, 0, rig.sent())));
    Probe::set_order(rig.loop, 0u, 0u);
    CHECK(!rig.loop.invariants_hold());
    Probe::set_order(rig.loop, 0u, 99u);
    CHECK(!rig.loop.invariants_hold()); }
  Rig rig;
  const BatchMark mark = rig.sent();
  CHECK(rig.loop.hand(rig.frame(rig.first, 0u, 0, mark)));
  CHECK(rig.loop.hand(rig.frame(rig.first, 1u, 0, mark)));
  Probe::set_order(rig.loop, 1u, 1u);
  CHECK(!rig.loop.invariants_hold()); }
