/* The windows one context draws on (research.md §6): the first make-current
 * binds the window as a new context finds it, a later one moves only the
 * bindings the application left on the window, framebuffer 0 and the
 * window's buffers are translated both ways, and gone windows are given up. */
#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/counted_wake_signal.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_thread.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "icd/opengl/client_array_state.hpp"
#include "icd/opengl/gl_present_loop.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "icd/opengl/pixel_format.hpp"
#include "icd/opengl/window_framebuffers.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"

#include "gl_protocol_ids.h"

#include <cstdint>
#include <cstring>

using av::GlContextOptions;
using av::GlContextRig;
using av::Status;
using av::glicd::ClientArrayState;
using av::glicd::GuestGlContext;
using av::glicd::PixelFormat;
using av::glicd::WindowFramebuffers;
using Windows = WindowFramebuffers;
namespace ids = av::glproto;

namespace av::glicd {

struct BackBufferPresenterProbe {
    static void set_window(BackBufferPresenter &presenter, void *window) { presenter.window_ = window; }
    static void drop_texture(BackBufferPresenter &presenter) { presenter.texture_ = 0u; }
};

struct WindowFramebuffersProbe {
    static void point_nowhere(WindowFramebuffers &windows, std::uint32_t slot) { windows.current_ = slot; }
    static void presented(WindowFramebuffers &windows, std::uint32_t slot) { windows.presented_ = slot; }
    static BackBufferPresenter &presenter(WindowFramebuffers &windows, std::uint32_t slot)
    {
        return windows.presenters_[slot].object();
    }
    static void copy_window(WindowFramebuffers &windows, BlockSource &memory, icd::LogSink &log, void *window)
    {
        static_cast<void>(windows.presenters_[5].construct(memory, log));
        BackBufferPresenterProbe::set_window(windows.presenters_[5].object(), window);
    }
};

}  // namespace av::glicd

namespace {

const PixelFormat doubled(24u, 8u, true, false, 0u);
const PixelFormat single(24u, 8u, false, false, 0u);

struct Rig {
    GlContextRig              context_rig;
    GuestGlContext            context{context_rig.connection(), context_rig.platform(), context_rig.send_lock(),
                                      context_rig.log(), context_rig.extensions(), context_rig.handles(), 1, GlContextOptions()};
    av::ScriptedWindowSurface surface;
    ClientArrayState          state;
    int                       dcs[10] = {};
    int                       windows[10] = {};
    Windows                   framebuffers{context_rig.platform(), context_rig.log()};

    Rig()
    {
        context_rig.host().gl_make("renderer", "4.6");
        CHECK_STATUS(context.make(), Status::ok);
        for (int index = 0; index < 10; index++) surface.add_dc(&dcs[index], &windows[index], 8u, 4u);
    }
    av::ClientTransport &wire() { return *context.wire(); }
    /* What reached the host: the context holds its calls in a batch until a
     * flush, so the held ones are sent first. */
    std::uint32_t        commands()
    {
        context.flush();
        return context_rig.kmt().commands();
    }
    std::uint32_t        id(std::uint32_t index) { return context_rig.kmt().command_id(index); }
    std::uint64_t        argument(std::uint32_t index, std::uint32_t nth)
    {
        av::WireReader reader(context_rig.kmt().command_bytes(index) + av::CommandHeader::encoded_size,
                              av::MockKmt::journal_bytes - av::CommandHeader::encoded_size);
        std::uint64_t  value = 0u;
        for (std::uint32_t at = 0u; at <= nth; at++) CHECK(reader.read_varint(value));
        return value;
    }
    Status current_on(int index, std::uint32_t first, const PixelFormat &format = doubled)
    {
        context_rig.host().gl_window(1u, 1u, first);
        return framebuffers.make_current_on(wire(), &windows[index], format, surface);
    }    /* A swap as the root makes one with no present thread: the frame onto
     * the window, then the window's new size followed. */
    bool swap(int index)
    {
        const bool shown = framebuffers.present(wire(), state, &dcs[index], &windows[index], surface, nullptr);
        framebuffers.follow(wire(), &windows[index], doubled, surface);
        return shown;
    }
};

}  // namespace

TEST(window_framebuffers, the_first_window_is_bound_with_its_viewport_as_a_new_context_finds_it)
{ Rig rig;
  CHECK(rig.framebuffers.window() == nullptr);
  CHECK_EQ(rig.framebuffers.name_of(0u), 0u);
  const std::uint32_t before = rig.commands();
  CHECK_STATUS(rig.current_on(0, 100u), Status::ok);
  CHECK(rig.framebuffers.invariants_hold());
  CHECK(rig.framebuffers.window() == &rig.windows[0]);
  CHECK_EQ(rig.framebuffers.held(), 1u);
  CHECK_EQ(rig.framebuffers.name_of(0u), 100u);
  CHECK_EQ(rig.framebuffers.name_of(7u), 7u);
  const std::uint32_t bound = before + 7u;
  CHECK_EQ(rig.commands() - before, 10u);
  CHECK_EQ(rig.id(bound), ids::kCmd_glBindFramebuffer);
  CHECK_EQ(rig.argument(bound, 0u), Windows::gl_framebuffer);
  CHECK_EQ(rig.argument(bound, 1u), 100u);
  CHECK_EQ(rig.id(bound + 1u), ids::kCmd_glViewport);
  CHECK_EQ(rig.argument(bound + 1u, 2u), 8u);
  CHECK_EQ(rig.argument(bound + 1u, 3u), 4u);
  CHECK_EQ(rig.id(bound + 2u), ids::kCmd_glScissor);
  /* The same window again sends nothing. */
  const std::uint32_t again = rig.commands();
  CHECK_STATUS(rig.framebuffers.make_current_on(rig.wire(), &rig.windows[0], doubled, rig.surface), Status::ok);
  CHECK_EQ(rig.commands(), again); }

/* A later window takes the bindings the application left on the window, and
 * only those; the viewport is the application's by then. */
TEST(window_framebuffers, a_later_window_moves_only_the_bindings_left_on_the_window)
{ Rig rig;
  CHECK_STATUS(rig.current_on(0, 100u), Status::ok);
  std::uint32_t moved = rig.commands();
  CHECK_STATUS(rig.current_on(1, 200u), Status::ok);
  CHECK_EQ(rig.commands() - moved, 9u);
  CHECK_EQ(rig.argument(moved + 7u, 0u), Windows::gl_draw_framebuffer);
  CHECK_EQ(rig.argument(moved + 7u, 1u), 200u);
  CHECK_EQ(rig.argument(moved + 8u, 0u), Windows::gl_read_framebuffer);
  CHECK_EQ(rig.framebuffers.name_of(0u), 200u);
  rig.framebuffers.bound(Windows::gl_draw_framebuffer, 5u);
  CHECK_STATUS(rig.framebuffers.make_current_on(rig.wire(), &rig.windows[0], doubled, rig.surface), Status::ok);
  moved = rig.commands();
  rig.framebuffers.bound(Windows::gl_framebuffer, 5u);
  rig.framebuffers.bound(Windows::gl_read_framebuffer, 0u);
  CHECK_STATUS(rig.framebuffers.make_current_on(rig.wire(), &rig.windows[1], doubled, rig.surface), Status::ok);
  CHECK_EQ(rig.commands() - moved, 1u);
  CHECK_EQ(rig.argument(moved, 0u), Windows::gl_read_framebuffer);
  moved = rig.commands();
  rig.framebuffers.bound(Windows::gl_read_framebuffer, 6u);
  rig.framebuffers.bound(0x1234u, 0u);
  CHECK_STATUS(rig.framebuffers.make_current_on(rig.wire(), &rig.windows[0], doubled, rig.surface), Status::ok);
  CHECK_EQ(rig.commands(), moved);
  CHECK(rig.framebuffers.invariants_hold()); }

TEST(window_framebuffers, a_ninth_window_is_no_space_and_a_refused_one_is_not_kept)
{ Rig rig;
  for (int index = 0; index < 8; index++)
    CHECK_STATUS(rig.current_on(index, 100u + 10u * static_cast<std::uint32_t>(index)), Status::ok);
  CHECK_EQ(rig.framebuffers.held(), 8u);
  CHECK_STATUS(rig.framebuffers.make_current_on(rig.wire(), &rig.windows[8], doubled, rig.surface), Status::no_space);
  CHECK(rig.framebuffers.window() == &rig.windows[7]);
  Rig refused;
  CHECK_STATUS(refused.current_on(0, 100u), Status::ok);
  int unknown = 0;
  CHECK_STATUS(refused.framebuffers.make_current_on(refused.wire(), &unknown, doubled, refused.surface),
               Status::invalid_parameter);
  CHECK_EQ(refused.framebuffers.held(), 1u);
  CHECK(refused.framebuffers.window() == &refused.windows[0]);
  /* A window already held keeps its presenter when a remake is refused. */
  refused.surface.resize(&refused.windows[0], Windows::max_windows * 2000u, 4u);
  CHECK_STATUS(refused.framebuffers.make_current_on(refused.wire(), &refused.windows[0], doubled, refused.surface),
               Status::invalid_parameter);
  CHECK_EQ(refused.framebuffers.held(), 1u);
  CHECK(refused.framebuffers.invariants_hold()); }

TEST(window_framebuffers, the_windows_buffers_are_its_colour_attachment_while_it_is_bound)
{ Rig rig;
  const std::uint32_t window_buffers[] = {Windows::gl_front_left, Windows::gl_back_left, Windows::gl_front,
                                          Windows::gl_back, Windows::gl_left, Windows::gl_front_and_back};
  for (const std::uint32_t buffer : window_buffers) {
    CHECK_EQ(rig.framebuffers.buffer_for(buffer, true), buffer);
  }
  CHECK_STATUS(rig.current_on(0, 100u), Status::ok);
  for (const std::uint32_t buffer : window_buffers) {
    CHECK_EQ(rig.framebuffers.buffer_for(buffer, true), Windows::gl_color_attachment0);
    CHECK_EQ(rig.framebuffers.buffer_for(buffer, false), Windows::gl_color_attachment0);
  }
  CHECK_EQ(rig.framebuffers.buffer_for(Windows::gl_none, true), Windows::gl_none);
  CHECK_EQ(rig.framebuffers.buffer_for(0x8CE1u, false), 0x8CE1u);
  rig.framebuffers.bound(Windows::gl_draw_framebuffer, 3u);
  CHECK_EQ(rig.framebuffers.buffer_for(Windows::gl_back, true), Windows::gl_back);
  CHECK_EQ(rig.framebuffers.buffer_for(Windows::gl_back, false), Windows::gl_color_attachment0);
  rig.framebuffers.bound(Windows::gl_read_framebuffer, 3u);
  CHECK_EQ(rig.framebuffers.buffer_for(Windows::gl_back, false), Windows::gl_back); }

/* What a query answers is what a window would: binding 0, GL_BACK or GL_FRONT. */
TEST(window_framebuffers, a_query_answers_as_the_window_would)
{ Rig rig;
  const auto attachment = static_cast<std::int32_t>(Windows::gl_color_attachment0);
  CHECK_EQ(rig.framebuffers.answer(Windows::gl_draw_buffer, attachment), attachment);
  CHECK_STATUS(rig.current_on(0, 100u), Status::ok);
  CHECK_EQ(rig.framebuffers.answer(Windows::gl_draw_framebuffer_binding, 100), 0);
  CHECK_EQ(rig.framebuffers.answer(Windows::gl_read_framebuffer_binding, 100), 0);
  CHECK_EQ(rig.framebuffers.answer(Windows::gl_draw_buffer, attachment), static_cast<std::int32_t>(Windows::gl_back));
  CHECK_EQ(rig.framebuffers.answer(Windows::gl_draw_buffer0, attachment), static_cast<std::int32_t>(Windows::gl_back));
  CHECK_EQ(rig.framebuffers.answer(Windows::gl_read_buffer, attachment), static_cast<std::int32_t>(Windows::gl_back));
  CHECK_EQ(rig.framebuffers.answer(Windows::gl_draw_buffer, 0), 0);
  CHECK_EQ(rig.framebuffers.answer(0x0B21u, attachment), attachment);
  rig.framebuffers.bound(Windows::gl_framebuffer, 9u);
  CHECK_EQ(rig.framebuffers.answer(Windows::gl_draw_framebuffer_binding, 9), 9);
  CHECK_EQ(rig.framebuffers.answer(Windows::gl_read_framebuffer_binding, 9), 9);
  CHECK_EQ(rig.framebuffers.answer(Windows::gl_draw_buffer, attachment), attachment);
  CHECK_EQ(rig.framebuffers.answer(Windows::gl_read_buffer, attachment), attachment);
  /* A single-buffered window's buffer is its front one. */
  Rig front;
  CHECK_STATUS(front.current_on(0, 100u, single), Status::ok);
  CHECK_EQ(front.framebuffers.answer(Windows::gl_draw_buffer, attachment), static_cast<std::int32_t>(Windows::gl_front));
  /* With the window gone the answer is the host's. */
  front.surface.close_window(&front.windows[0]);
  front.framebuffers.drop_gone(front.wire(), front.surface);
  CHECK_EQ(front.framebuffers.answer(Windows::gl_read_buffer, attachment), attachment); }

TEST(window_framebuffers, a_swap_presents_the_window_and_a_resize_rebinds_what_was_left_on_it)
{ Rig rig;
  CHECK(!rig.framebuffers.present(rig.wire(), rig.state, &rig.dcs[0], nullptr, rig.surface, nullptr));
  rig.framebuffers.follow(rig.wire(), nullptr, doubled, rig.surface);
  rig.framebuffers.hand_over(av::BatchMark());
  CHECK(!rig.swap(0));
  CHECK_STATUS(rig.current_on(0, 100u), Status::ok);
  CHECK_STATUS(rig.current_on(1, 200u), Status::ok);
  rig.context_rig.host().gl_bytes(nullptr, 0u);
  std::uint32_t swapped = rig.commands();
  CHECK(rig.swap(1));
  CHECK_EQ(rig.commands() - swapped, 1u);
  CHECK_EQ(rig.surface.blits(), 1u);
  /* The current window grows: made again, and both bindings follow it. */
  rig.surface.resize(&rig.windows[1], 16u, 8u);
  rig.context_rig.host().gl_bytes(nullptr, 0u);
  rig.context_rig.host().gl_window(1u, 1u, 300u);
  swapped = rig.commands();
  CHECK(rig.swap(1));
  CHECK_EQ(rig.commands() - swapped, 1u + 3u + 7u + 2u);
  CHECK_EQ(rig.argument(swapped + 11u, 1u), 300u);
  CHECK_EQ(rig.argument(swapped + 12u, 0u), Windows::gl_read_framebuffer);
  CHECK_EQ(rig.framebuffers.name_of(0u), 300u);
  /* A window the application moved off, or one not current, is only made again. */
  rig.framebuffers.bound(Windows::gl_draw_framebuffer, 4u);
  rig.surface.resize(&rig.windows[1], 8u, 8u);
  rig.context_rig.host().gl_bytes(nullptr, 0u);
  rig.context_rig.host().gl_window(1u, 1u, 400u);
  swapped = rig.commands();
  CHECK(rig.swap(1));
  CHECK_EQ(rig.commands() - swapped, 1u + 3u + 7u + 1u);
  CHECK_EQ(rig.argument(swapped + 11u, 0u), Windows::gl_read_framebuffer);
  rig.framebuffers.bound(Windows::gl_framebuffer, 0u);
  rig.surface.resize(&rig.windows[0], 4u, 4u);
  rig.context_rig.host().gl_bytes(nullptr, 0u);
  rig.context_rig.host().gl_window(1u, 1u, 500u);
  swapped = rig.commands();
  CHECK(rig.swap(0));
  CHECK_EQ(rig.commands() - swapped, 1u + 3u + 7u);
  /* The current window, drawn on there and read elsewhere. */
  rig.framebuffers.bound(Windows::gl_framebuffer, 0u);
  rig.framebuffers.bound(Windows::gl_read_framebuffer, 6u);
  rig.surface.resize(&rig.windows[1], 6u, 6u);
  rig.context_rig.host().gl_bytes(nullptr, 0u);
  rig.context_rig.host().gl_window(1u, 1u, 600u);
  swapped = rig.commands();
  CHECK(rig.swap(1));
  CHECK_EQ(rig.commands() - swapped, 1u + 3u + 7u + 1u);
  CHECK_EQ(rig.argument(swapped + 11u, 0u), Windows::gl_draw_framebuffer);
  rig.framebuffers.bound(Windows::gl_read_framebuffer, 0u);
  /* A window whose remake is refused keeps presenting what it had. */
  rig.surface.resize(&rig.windows[1], Windows::max_windows * 2000u, 8u);
  rig.context_rig.host().gl_bytes(nullptr, 0u);
  CHECK(rig.swap(1));
  CHECK_EQ(rig.framebuffers.name_of(0u), 600u);
  CHECK(rig.framebuffers.invariants_hold()); }

/* Feature 051: the host has no front buffer - the window is a framebuffer
 * object there - so GL_FRONT names the frame the swap put on the window, which
 * the guest still holds. A read of it is answered from there; a read of
 * anything else is the host's. */
TEST(window_framebuffers, a_read_of_the_windows_front_buffer_is_answered_without_the_host)
{ Rig rig;
  CHECK_STATUS(rig.current_on(0, 100u), Status::ok);
  std::uint8_t pixels[64] = {};
  /* Nothing presented yet, and the read buffer is GL_BACK. */
  CHECK(!rig.framebuffers.read_front(0, 0, 1, 1, 0x80E1u, 0x1401u, pixels));
  rig.context_rig.host().gl_bytes(nullptr, 0u);
  CHECK(rig.swap(0));
  /* Still the back buffer: that read is the host's. */
  CHECK(!rig.framebuffers.read_front(0, 0, 1, 1, 0x80E1u, 0x1401u, pixels));
  rig.framebuffers.reading(0x0404u /* GL_FRONT */);
  const std::uint32_t commands = rig.commands();
  CHECK(rig.framebuffers.read_front(0, 0, 1, 1, 0x80E1u, 0x1401u, pixels));
  CHECK_EQ(rig.commands(), commands);
  /* A rectangle past the frame, and a format no present produced, are the
   * host's too. */
  CHECK(!rig.framebuffers.read_front(0, 0, 4096, 4096, 0x80E1u, 0x1401u, pixels));
  CHECK(!rig.framebuffers.read_front(0, 0, 1, 1, 0x1907u /* GL_RGB */, 0x1401u, pixels));
  rig.framebuffers.reading(0x0405u /* GL_BACK */);
  CHECK(!rig.framebuffers.read_front(0, 0, 1, 1, 0x80E1u, 0x1401u, pixels));
  /* A read the application moved off the window is the host's, whatever the
   * read buffer is called. */
  rig.framebuffers.reading(0x0400u /* GL_FRONT_LEFT */);
  CHECK(rig.framebuffers.read_front(0, 0, 1, 1, 0x80E1u, 0x1401u, pixels));
  rig.framebuffers.bound(Windows::gl_read_framebuffer, 4u);
  CHECK(!rig.framebuffers.read_front(0, 0, 1, 1, 0x80E1u, 0x1401u, pixels));
  CHECK(rig.framebuffers.invariants_hold()); }

/* With no window current at all there is no frame to read - including for an
 * application that bound framebuffer 0 before it had one. */
TEST(window_framebuffers, a_front_read_with_no_window_current_is_the_hosts)
{ Rig rig;
  std::uint8_t pixels[16] = {};
  rig.framebuffers.reading(0x0404u /* GL_FRONT */);
  CHECK(!rig.framebuffers.read_front(0, 0, 1, 1, 0x80E1u, 0x1401u, pixels));
  rig.framebuffers.bound(Windows::gl_read_framebuffer, 0u);
  CHECK(!rig.framebuffers.read_front(0, 0, 1, 1, 0x80E1u, 0x1401u, pixels));
  CHECK(rig.framebuffers.invariants_hold()); }

TEST(window_framebuffers, gone_windows_are_given_up_and_release_gives_up_all)
{ Rig rig;
  CHECK_STATUS(rig.current_on(0, 100u), Status::ok);
  CHECK_STATUS(rig.current_on(1, 200u), Status::ok);
  CHECK_STATUS(rig.current_on(2, 300u), Status::ok);
  rig.surface.close_window(&rig.windows[0]);
  rig.surface.close_window(&rig.windows[2]);
  const std::uint32_t dropped = rig.commands();
  rig.framebuffers.drop_gone(rig.wire(), rig.surface);
  CHECK_EQ(rig.commands() - dropped, 6u);
  CHECK_EQ(rig.framebuffers.held(), 1u);
  CHECK(rig.framebuffers.window() == nullptr);
  CHECK_EQ(rig.framebuffers.name_of(0u), 0u);
  CHECK(rig.framebuffers.invariants_hold());
  const std::uint32_t released = rig.commands();
  rig.framebuffers.release(&rig.wire());
  CHECK_EQ(rig.commands() - released, 3u);
  CHECK_EQ(rig.framebuffers.held(), 0u);
  rig.framebuffers.release(nullptr);
  CHECK_EQ(rig.commands() - released, 3u); }

TEST(window_framebuffers, reports_its_bindings_and_each_window)
{ Rig rig;
  CHECK_STATUS(rig.current_on(0, 100u), Status::ok);
  CHECK_STATUS(rig.current_on(1, 200u), Status::ok);
  rig.framebuffers.bound(Windows::gl_draw_framebuffer, 5u);
  char bytes[512];
  av::StateWriter writer(bytes, sizeof bytes);
  rig.framebuffers.report(writer);
  CHECK(std::strstr(writer.dump(), "windows/0 held=2 draws_window=false reads_window=true\n") == writer.dump());
  CHECK(std::strstr(writer.dump(), "windows/0/presenter/0 hwnd=") != nullptr);
  CHECK(std::strstr(writer.dump(), "windows/0/presenter/1 hwnd=") != nullptr); }

TEST(window_framebuffers_invariants, fail_for_a_current_window_not_held_a_window_held_twice_or_a_broken_presenter)
{ using Probe = av::glicd::WindowFramebuffersProbe;
  Rig nowhere;
  Probe::point_nowhere(nowhere.framebuffers, 3u);
  CHECK(!nowhere.framebuffers.invariants_hold());
  Probe::point_nowhere(nowhere.framebuffers, Windows::max_windows + 1u);
  CHECK(!nowhere.framebuffers.invariants_hold());
  Probe::point_nowhere(nowhere.framebuffers, Windows::max_windows);
  CHECK(nowhere.framebuffers.invariants_hold());
  Rig twice;
  CHECK_STATUS(twice.current_on(0, 100u), Status::ok);
  CHECK_STATUS(twice.current_on(1, 200u), Status::ok);
  Probe::copy_window(twice.framebuffers, twice.context_rig.platform(), twice.context_rig.log(), &twice.windows[1]);
  CHECK(!twice.framebuffers.invariants_hold());
  Rig broken;
  CHECK_STATUS(broken.current_on(0, 100u), Status::ok);
  av::glicd::BackBufferPresenterProbe::drop_texture(Probe::presenter(broken.framebuffers, 0u));
  CHECK(!broken.framebuffers.invariants_hold()); }

/* The frame a present read for the thread is handed over once, with the
 * batch named; a window gone in between hands nothing, and neither does a
 * hand-over with no present before it. */
TEST(window_framebuffers, a_frame_read_for_the_thread_is_handed_over_once)
{ Rig                      rig;
  av::ScriptedThread       thread;
  av::CountedWakeSignal    handed;
  av::CountedWakeSignal    shown;
  av::LockTrace            trace;
  av::RecordingThreadLock  lock{trace, 'P'};
  av::RecordingLog         log;
  av::glicd::GlPresentLoop loop{thread, handed, shown, lock, rig.context_rig.kmt(), rig.surface, log};
  CHECK_STATUS(rig.current_on(0, 100u), Status::ok);
  CHECK(rig.framebuffers.present(rig.wire(), rig.state, &rig.dcs[0], &rig.windows[0], rig.surface, &loop));
  rig.context.flush();
  rig.framebuffers.hand_over(rig.context.mark());
  rig.framebuffers.hand_over(rig.context.mark());
  CHECK_EQ(loop.queued(), 1u);
  CHECK(thread.step());
  CHECK_EQ(rig.surface.blits(), 1u);
  CHECK(rig.framebuffers.present(rig.wire(), rig.state, &rig.dcs[0], &rig.windows[0], rig.surface, &loop));
  rig.surface.close_window(&rig.windows[0]);
  rig.framebuffers.drop_gone(rig.wire(), rig.surface);
  rig.framebuffers.hand_over(rig.context.mark());
  CHECK_EQ(loop.queued(), 0u);
  CHECK(rig.framebuffers.invariants_hold());
  rig.framebuffers.release(nullptr); }

TEST(window_framebuffers_invariants, fail_for_a_presented_window_past_the_table)
{ Rig rig;
  av::glicd::WindowFramebuffersProbe::presented(rig.framebuffers, Windows::max_windows + 1u);
  CHECK(!rig.framebuffers.invariants_hold());
  av::glicd::WindowFramebuffersProbe::presented(rig.framebuffers, Windows::max_windows);
  CHECK(rig.framebuffers.invariants_hold()); }
