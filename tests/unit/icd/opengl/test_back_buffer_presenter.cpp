/* One window's back buffer (research.md §6): what a first frame, a resize and
 * a multisampled frame send to a scripted host, what reaches the window - the
 * host's bytes, bottom row first - and every refusal on the way. */
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
#include "icd/opengl/back_buffer_presenter.hpp"
#include "icd/opengl/client_array_state.hpp"
#include "icd/opengl/gl_present_loop.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "icd/opengl/pixel_format.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"

#include "gl_protocol_ids.h"

#include <cstdint>
#include <cstring>

using av::GlContextOptions;
using av::GlContextRig;
using av::Status;
using av::glicd::BackBufferPresenter;
using av::glicd::ClientArrayState;
using av::glicd::GuestGlContext;
using av::glicd::PixelFormat;
using Presenter = BackBufferPresenter;
namespace ids = av::glproto;

namespace av::glicd {

struct BackBufferPresenterProbe {
    static std::uint8_t *swap_pixels(BackBufferPresenter &presenter, std::uint8_t *pixels)
    {
        std::uint8_t *const held = presenter.pixels_;
        presenter.pixels_ = pixels;
        return held;
    }
    static void shrink(BackBufferPresenter &presenter) { presenter.capacity_ = 1u; }
    static void drop_resolve(BackBufferPresenter &presenter) { presenter.framebuffers_[1] = 0u; }
    static void drop_depth(BackBufferPresenter &presenter) { presenter.renderbuffers_[1] = 0u; }
    static void drop_framebuffer(BackBufferPresenter &presenter) { presenter.framebuffers_[0] = 0u; }
    static void drop_texture(BackBufferPresenter &presenter) { presenter.texture_ = 0u; }
    static void drop_colour(BackBufferPresenter &presenter) { presenter.renderbuffers_[0] = 0u; }
    static std::uint8_t *room(BackBufferPresenter &presenter)
    {
        return static_cast<std::uint8_t *>(presenter.room_.data());
    }
    static std::size_t room_bytes(BackBufferPresenter &presenter) { return presenter.room_.bytes(); }
    static void set_pending(BackBufferPresenter &presenter, std::uint32_t slot) { presenter.pending_ = slot; }
    static void drop_arena(BackBufferPresenter &presenter) { presenter.arena_ = nullptr; }
};

}  // namespace av::glicd

namespace {

const PixelFormat plain_d24s8(24u, 8u, true, false, 0u);
const PixelFormat multisampled_d32_srgb(32u, 0u, false, true, 4u);
const PixelFormat bare(0u, 0u, true, false, 0u);

/* A made context whose transport the presenter sends through, a window of
 * 8x4 on one DC, and the client state a swap reads the pack state from. */
struct Rig {
    GlContextRig               context_rig;
    GuestGlContext             context{context_rig.connection(), context_rig.platform(), context_rig.send_lock(),
                                       context_rig.log(), context_rig.extensions(), context_rig.handles(), 1, GlContextOptions()};
    av::ScriptedWindowSurface  surface;
    ClientArrayState           state;
    int                        dc = 0;
    int                        window = 0;
    Presenter                  presenter{context_rig.platform(), context_rig.log()};

    explicit Rig(bool arena = true, std::uint32_t width = 8u, std::uint32_t height = 4u)
    {
        if (!arena) {
            context_rig.kmt().fail_at(av::MockKmt::Call::create_allocation, 5u, av::MockKmt::invalid_parameter);
            host().gl_string("vendor");
            host().gl_string("renderer");
            host().gl_string("4.6");
            host().gl_string("4.60");
            for (int list = 0; list < 3; list++) host().gl_integer(0);
            /* GL_MAJOR_VERSION, GL_MINOR_VERSION, GL_CONTEXT_PROFILE_MASK. */
            host().gl_integer(4);
            host().gl_integer(6);
            host().gl_integer(2);
        } else {
            host().gl_make("renderer", "4.6");
        }
        CHECK_STATUS(context.make(), Status::ok);
        surface.add_dc(&dc, &window, width, height);
    }
    av::ScriptedHost    &host() { return context_rig.host(); }
    av::MockKmt         &kmt() { return context_rig.kmt(); }
    av::ClientTransport &wire() { return *context.wire(); }
    /* What reached the host: the context holds its calls in a batch until a
     * flush, so the held ones are sent first. */
    std::uint32_t        commands()
    {
        context.flush();
        return kmt().commands();
    }
    std::uint32_t        id(std::uint32_t index)
    {
        context.flush();
        return kmt().command_id(index);
    }
    /* The `nth` varint argument of command `index`. */
    std::uint64_t argument(std::uint32_t index, std::uint32_t nth)
    {
        context.flush();
        const std::uint8_t *bytes = kmt().command_bytes(index);
        CHECK(bytes != nullptr);
        if (bytes == nullptr) return 0u;
        av::WireReader      reader(bytes + av::CommandHeader::encoded_size,
                                   av::MockKmt::journal_bytes - av::CommandHeader::encoded_size);
        std::uint64_t       value = 0u;
        for (std::uint32_t at = 0u; at <= nth; at++) CHECK(reader.read_varint(value));
        return value;
    }
};

void pattern(std::uint8_t *bytes, std::size_t size)
{
    for (std::size_t at = 0u; at < size; at++) bytes[at] = static_cast<std::uint8_t>(at * 7u + 1u);
}

using Probe = av::glicd::BackBufferPresenterProbe;

/* The present thread a swap hands its frames to, stepped by the test. */
struct Presents {
    av::ScriptedThread       thread;
    av::CountedWakeSignal    handed;
    av::CountedWakeSignal    shown;
    av::LockTrace            trace;
    av::RecordingThreadLock  lock{trace, 'P'};
    av::RecordingLog         log;
    av::glicd::GlPresentLoop loop;

    explicit Presents(Rig &rig) : loop(thread, handed, shown, lock, rig.kmt(), rig.surface, log), rig_(rig) {}
    /* The presenter lets go of the thread before the thread goes, as the
     * ICD's root goes before its present loop. */
    ~Presents() { rig_.presenter.release(nullptr); }

private:
    Rig &rig_;
};

/* A one-way glGetTextureSubImage into the arena: its eleven scalars, the
 * blob mode, the byte count and the offset the host writes at. */
struct IntoBlob {
    std::uint64_t texture = 0u;
    std::uint64_t width = 0u;
    std::uint64_t height = 0u;
    std::uint8_t  mode = 0u;
    std::uint64_t bytes = 0u;
    std::uint64_t at = 0u;
};

IntoBlob into_blob(Rig &rig, std::uint32_t index)
{
    CHECK_EQ(rig.id(index), ids::kCmd_glGetTextureSubImage);
    av::WireReader reader(rig.kmt().command_bytes(index) + av::CommandHeader::encoded_size,
                          av::MockKmt::journal_bytes - av::CommandHeader::encoded_size);
    std::uint64_t  scalars[11] = {};
    for (std::uint64_t &scalar : scalars) CHECK(reader.read_varint(scalar));
    IntoBlob read;
    read.texture = scalars[0];
    read.width = scalars[5];
    read.height = scalars[6];
    CHECK(reader.read_pod(read.mode));
    CHECK(reader.read_varint(read.bytes));
    CHECK(reader.read_varint(read.at));
    return read;
}

}  // namespace

TEST(back_buffer_presenter, a_first_frame_makes_the_framebuffer_and_puts_the_hosts_pixels_on_the_window)
{ Rig rig;
  CHECK(rig.presenter.invariants_hold());
  CHECK(!rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, nullptr));
  rig.host().gl_window(1u, 1u);
  const std::uint32_t made = rig.commands();
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  CHECK(rig.presenter.invariants_hold());
  CHECK_EQ(rig.presenter.framebuffer(), 100u);
  CHECK(rig.presenter.window() == &rig.window);
  CHECK_EQ(rig.presenter.width(), 8u);
  CHECK_EQ(rig.presenter.height(), 4u);
  CHECK(rig.presenter.double_buffered());
  const std::uint32_t expected[] = {ids::kCmd_glCreateFramebuffers, ids::kCmd_glCreateTextures,
                                    ids::kCmd_glCreateRenderbuffers, ids::kCmd_glTextureStorage2D,
                                    ids::kCmd_glNamedFramebufferTexture,
                                    ids::kCmd_glNamedRenderbufferStorageMultisample,
                                    ids::kCmd_glNamedFramebufferRenderbuffer};
  CHECK_EQ(rig.commands() - made, 7u);
  for (std::uint32_t at = 0u; at < 7u; at++) CHECK_EQ(rig.id(made + at), expected[at]);
  /* glTextureStorage2D(texture 101, 1 level, GL_RGBA8, 8, 4). */
  CHECK_EQ(rig.argument(made + 3u, 0u), 101u);
  CHECK_EQ(rig.argument(made + 3u, 2u), Presenter::gl_rgba8);
  CHECK_EQ(rig.argument(made + 3u, 3u), 8u);
  CHECK_EQ(rig.argument(made + 3u, 4u), 4u);
  /* The texture on the framebuffer's colour attachment, the D24S8
   * renderbuffer 102 on its depth-stencil one. */
  CHECK_EQ(rig.argument(made + 4u, 0u), 100u);
  CHECK_EQ(rig.argument(made + 4u, 1u), Presenter::gl_color_attachment0);
  CHECK_EQ(rig.argument(made + 5u, 0u), 102u);
  CHECK_EQ(rig.argument(made + 5u, 1u), 0u);
  CHECK_EQ(rig.argument(made + 5u, 2u), Presenter::gl_depth24_stencil8);
  CHECK_EQ(rig.argument(made + 6u, 1u), Presenter::gl_depth_stencil_attachment);
  /* The same window at the same size is made once. */
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  CHECK_EQ(rig.commands() - made, 7u);
  std::uint8_t frame[8u * 4u * 4u];
  pattern(frame, sizeof frame);
  rig.host().gl_bytes(frame, sizeof frame);
  const std::uint32_t read = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, nullptr));
  CHECK_EQ(rig.commands() - read, 1u);
  CHECK_EQ(rig.id(read), ids::kCmd_glGetTextureSubImage);
  CHECK_EQ(rig.argument(read, 0u), 101u);
  CHECK_EQ(rig.surface.blits(), 1u);
  CHECK(rig.surface.last_blit_dc() == &rig.dc);
  CHECK_EQ(rig.surface.last_blit_width(), 8u);
  CHECK_EQ(rig.surface.last_blit_height(), 4u);
  CHECK(std::memcmp(rig.surface.last_blit_bytes(), frame, sizeof frame) == 0);
  CHECK(rig.presenter.invariants_hold()); }

TEST(back_buffer_presenter, a_resize_deletes_the_old_objects_and_makes_new_ones)
{ Rig rig;
  rig.host().gl_window(1u, 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  rig.surface.resize(&rig.window, 16u, 2u);
  rig.host().gl_window(1u, 1u, 200u);
  const std::uint32_t resized = rig.commands();
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  CHECK_EQ(rig.id(resized), ids::kCmd_glDeleteFramebuffers);
  CHECK_EQ(rig.id(resized + 1u), ids::kCmd_glDeleteTextures);
  CHECK_EQ(rig.id(resized + 2u), ids::kCmd_glDeleteRenderbuffers);
  CHECK_EQ(rig.id(resized + 3u), ids::kCmd_glCreateFramebuffers);
  CHECK_EQ(rig.presenter.framebuffer(), 200u);
  CHECK_EQ(rig.presenter.width(), 16u);
  CHECK_EQ(rig.presenter.height(), 2u);
  CHECK(rig.presenter.invariants_hold());
  /* Smaller again keeps the pixels it has. */
  const std::size_t live = rig.context_rig.platform().live_allocations();
  rig.surface.resize(&rig.window, 2u, 2u);
  rig.host().gl_window(1u, 1u, 300u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  CHECK_EQ(rig.context_rig.platform().live_allocations(), live);
  CHECK_EQ(rig.presenter.framebuffer(), 300u);
  /* A different window of the same size is made again too. */
  int other = 0;
  int other_dc = 0;
  rig.surface.add_dc(&other_dc, &other, 2u, 2u);
  rig.host().gl_window(1u, 1u, 400u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &other, plain_d24s8, rig.surface), Status::ok);
  CHECK_EQ(rig.presenter.framebuffer(), 400u);
  CHECK(rig.presenter.window() == &other);
  /* Only the height changed. */
  rig.surface.resize(&other, 2u, 3u);
  rig.host().gl_window(1u, 1u, 500u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &other, plain_d24s8, rig.surface), Status::ok);
  CHECK_EQ(rig.presenter.framebuffer(), 500u);
  CHECK_EQ(rig.presenter.height(), 3u); }

/* WGL_ARB_multisample's formats: a multisampled colour renderbuffer on the
 * framebuffer, resolved into the texture on a second framebuffer before it
 * is read. */
TEST(back_buffer_presenter, a_multisampled_frame_is_resolved_before_it_is_read)
{ Rig rig;
  rig.host().gl_window(2u, 2u);
  const std::uint32_t made = rig.commands();
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, multisampled_d32_srgb, rig.surface), Status::ok);
  CHECK(rig.presenter.invariants_hold());
  CHECK(!rig.presenter.double_buffered());
  CHECK_EQ(rig.argument(made, 0u), 2u);
  CHECK_EQ(rig.argument(made + 2u, 0u), 2u);
  /* Storage: the texture, then the colour renderbuffer 103 and the depth one
   * 104, both of four samples. */
  CHECK_EQ(rig.id(made + 3u), ids::kCmd_glTextureStorage2D);
  CHECK_EQ(rig.id(made + 4u), ids::kCmd_glNamedFramebufferTexture);
  /* The texture 102 is the resolve framebuffer's (101) colour. */
  CHECK_EQ(rig.argument(made + 4u, 0u), 101u);
  CHECK_EQ(rig.argument(made + 4u, 2u), 102u);
  CHECK_EQ(rig.id(made + 5u), ids::kCmd_glNamedRenderbufferStorageMultisample);
  CHECK_EQ(rig.argument(made + 5u, 0u), 103u);
  CHECK_EQ(rig.argument(made + 5u, 1u), 4u);
  CHECK_EQ(rig.id(made + 6u), ids::kCmd_glNamedFramebufferRenderbuffer);
  CHECK_EQ(rig.argument(made + 6u, 0u), 100u);
  CHECK_EQ(rig.argument(made + 6u, 3u), 103u);
  CHECK_EQ(rig.argument(made + 7u, 0u), 104u);
  CHECK_EQ(rig.argument(made + 7u, 2u), Presenter::gl_depth_component32f);
  CHECK_EQ(rig.argument(made + 8u, 1u), Presenter::gl_depth_attachment);
  rig.host().gl_bytes(nullptr, 0u);
  const std::uint32_t read = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, nullptr));
  CHECK_EQ(rig.id(read), ids::kCmd_glBlitNamedFramebuffer);
  CHECK_EQ(rig.argument(read, 0u), 100u);
  CHECK_EQ(rig.argument(read, 1u), 101u);
  CHECK_EQ(rig.id(read + 1u), ids::kCmd_glGetTextureSubImage);
  CHECK_EQ(rig.surface.blits(), 1u); }

/* WGL_ARB_framebuffer_sRGB's formats store sRGB colour, which is what the
 * window is shown. */
TEST(back_buffer_presenter, an_srgb_format_stores_srgb_colour)
{ Rig rig;
  rig.host().gl_window(2u, 2u);
  const std::uint32_t made = rig.commands();
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, multisampled_d32_srgb, rig.surface), Status::ok);
  CHECK_EQ(rig.argument(made + 3u, 2u), Presenter::gl_srgb8_alpha8);
  CHECK_EQ(rig.argument(made + 5u, 2u), Presenter::gl_srgb8_alpha8); }

TEST(back_buffer_presenter, a_format_without_depth_makes_no_renderbuffer)
{ Rig rig;
  rig.host().gl_window(1u, 0u);
  const std::uint32_t made = rig.commands();
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, bare, rig.surface), Status::ok);
  CHECK_EQ(rig.commands() - made, 4u);
  CHECK_EQ(rig.id(made + 2u), ids::kCmd_glTextureStorage2D);
  CHECK(rig.presenter.invariants_hold());
  CHECK_EQ(rig.kmt().queued_answers(), 0u); }

TEST(back_buffer_presenter, a_0x0_window_has_storage_and_shows_nothing)
{ Rig rig(true, 0u, 0u);
  rig.host().gl_window(1u, 1u);
  const std::uint32_t made = rig.commands();
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  CHECK_EQ(rig.argument(made + 3u, 3u), 1u);
  CHECK_EQ(rig.argument(made + 3u, 4u), 1u);
  CHECK(rig.presenter.invariants_hold());
  const std::uint32_t swapped = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, nullptr));
  CHECK_EQ(rig.commands(), swapped);
  CHECK_EQ(rig.surface.blits(), 0u);
  Rig wide(true, 3u, 0u);
  wide.host().gl_window(1u, 1u);
  CHECK_STATUS(wide.presenter.prepare(wide.wire(), &wide.window, plain_d24s8, wide.surface), Status::ok);
  CHECK(wide.presenter.present(wide.wire(), wide.state, &wide.dc, wide.surface, nullptr));
  CHECK_EQ(wide.surface.blits(), 0u); }

TEST(back_buffer_presenter, refuses_a_window_that_is_gone_or_too_large)
{ Rig rig;
  int unknown = 0;
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &unknown, plain_d24s8, rig.surface), Status::invalid_parameter);
  rig.surface.resize(&rig.window, Presenter::max_edge + 1u, 4u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::invalid_parameter);
  rig.surface.resize(&rig.window, 4u, Presenter::max_edge + 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::invalid_parameter);
  CHECK_EQ(rig.presenter.framebuffer(), 0u);
  CHECK(rig.presenter.invariants_hold()); }

/* Whichever name the host does not give, nothing is kept and what it did give
 * is deleted again. */
TEST(back_buffer_presenter, a_name_the_host_does_not_give_keeps_nothing)
{ struct Case { const PixelFormat *format; std::uint32_t framebuffers; bool texture; std::uint32_t renderbuffers; };
  const Case cases[] = {
      {&plain_d24s8, 0u, true, 1u},          {&plain_d24s8, 1u, false, 1u},
      {&plain_d24s8, 1u, true, 0u},          {&multisampled_d32_srgb, 1u, true, 2u},
      {&multisampled_d32_srgb, 2u, true, 1u}, {&bare, 0u, true, 0u},
  };
  for (const Case &broken : cases) {
    Rig rig;
    const std::size_t live = rig.context_rig.platform().live_allocations();
    const std::uint32_t names[6] = {100u, 101u, 102u, 103u, 104u, 105u};
    rig.host().gl_bytes(reinterpret_cast<const std::uint8_t *>(names), broken.framebuffers * 4u);
    rig.host().gl_bytes(reinterpret_cast<const std::uint8_t *>(names + 2), broken.texture ? 4u : 0u);
    if (broken.format->depth_bits() != 0u)
      rig.host().gl_bytes(reinterpret_cast<const std::uint8_t *>(names + 3), broken.renderbuffers * 4u);
    const std::uint32_t made = rig.commands();
    CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, *broken.format, rig.surface), Status::no_memory);
    CHECK_EQ(rig.presenter.framebuffer(), 0u);
    CHECK(rig.presenter.invariants_hold());
    CHECK_EQ(rig.context_rig.platform().live_allocations(), live);
    /* Something was named, so the partial set is deleted. */
    CHECK_EQ(rig.id(rig.commands() - 1u), ids::kCmd_glDeleteRenderbuffers);
    CHECK(rig.commands() - made >= 5u);
  }
  Rig nothing;
  nothing.host().gl_bytes(nullptr, 0u);
  nothing.host().gl_bytes(nullptr, 0u);
  const std::uint32_t made = nothing.commands();
  CHECK_STATUS(nothing.presenter.prepare(nothing.wire(), &nothing.window, bare, nothing.surface), Status::no_memory);
  CHECK_EQ(nothing.commands() - made, 2u); }

TEST(back_buffer_presenter, pixels_it_cannot_have_are_no_memory)
{ Rig rig;
  rig.context_rig.platform().fail_allocation_at(1u);
  const std::uint32_t made = rig.commands();
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::no_memory);
  CHECK_EQ(rig.commands(), made);
  CHECK_EQ(rig.presenter.framebuffer(), 0u);
  CHECK(rig.presenter.invariants_hold()); }

/* A read-back writes where the application's pack state says, so a pack
 * buffer or pack state that would move the rows is set aside for the read
 * and the application's put back after it. */
TEST(back_buffer_presenter, the_applications_pack_state_is_set_aside_for_the_read)
{ Rig rig(true, 3u, 2u);
  rig.host().gl_window(1u, 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  rig.state.bind_buffer(ClientArrayState::pixel_pack_buffer, 9u);
  CHECK(rig.state.store_pixel(Presenter::gl_pack_skip_rows, 2));
  rig.host().gl_bytes(nullptr, 0u);
  std::uint32_t read = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, nullptr));
  CHECK_EQ(rig.commands() - read, 15u);
  CHECK_EQ(rig.id(read), ids::kCmd_glBindBuffer);
  CHECK_EQ(rig.argument(read, 1u), 0u);
  CHECK_EQ(rig.id(read + 1u), ids::kCmd_glPixelStorei);
  CHECK_EQ(rig.argument(read + 1u, 0u), Presenter::gl_pack_alignment);
  CHECK_EQ(rig.argument(read + 1u, 1u), 4u);
  CHECK_EQ(rig.argument(read + 3u, 0u), Presenter::gl_pack_skip_rows);
  CHECK_EQ(rig.argument(read + 3u, 1u), 0u);
  CHECK_EQ(rig.id(read + 7u), ids::kCmd_glGetTextureSubImage);
  CHECK_EQ(rig.argument(read + 8u, 1u), 9u);
  CHECK_EQ(rig.argument(read + 11u, 1u), 2u);
  /* An odd width under 8-byte alignment pads rows: set aside too. */
  rig.state.bind_buffer(ClientArrayState::pixel_pack_buffer, 0u);
  CHECK(rig.state.store_pixel(Presenter::gl_pack_skip_rows, 0));
  CHECK(rig.state.store_pixel(Presenter::gl_pack_alignment, 8));
  rig.host().gl_bytes(nullptr, 0u);
  read = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, nullptr));
  CHECK_EQ(rig.commands() - read, 15u);
  CHECK_EQ(rig.argument(read + 9u, 1u), 8u);
  /* Plain again: the read alone. */
  CHECK(rig.state.store_pixel(Presenter::gl_pack_alignment, 1));
  rig.host().gl_bytes(nullptr, 0u);
  read = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, nullptr));
  CHECK_EQ(rig.commands() - read, 1u); }

/* Without a blob arena every read is a reply, which holds 48 KiB. */
TEST(back_buffer_presenter, a_tall_frame_is_read_in_rows_a_reply_holds)
{ Rig rig(false, 64u, 200u);
  rig.host().gl_window(1u, 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  rig.host().gl_bytes(nullptr, 0u);
  rig.host().gl_bytes(nullptr, 0u);
  const std::uint32_t read = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, nullptr));
  CHECK_EQ(rig.commands() - read, 2u);
  /* 48 KiB is 192 rows of 64 pixels; the rest is 8. */
  CHECK_EQ(rig.argument(read, 3u), 0u);
  CHECK_EQ(rig.argument(read, 6u), 192u);
  CHECK_EQ(rig.argument(read + 1u, 3u), 192u);
  CHECK_EQ(rig.argument(read + 1u, 6u), 8u); }

TEST(back_buffer_presenter, a_refused_blit_is_logged_once_and_the_swap_happened)
{ Rig rig;
  rig.host().gl_window(1u, 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  for (int frame = 0; frame < 2; frame++) {
    rig.surface.refuse_next_blit();
    rig.host().gl_bytes(nullptr, 0u);
    CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, nullptr));
  }
  CHECK_EQ(rig.context_rig.log().holding("the window's DC refused a frame of 8x4"), 1u); }

TEST(back_buffer_presenter, release_deletes_through_a_wire_and_frees_without_one)
{ Rig rig;
  rig.host().gl_window(1u, 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  const std::uint32_t released = rig.commands();
  rig.presenter.release(&rig.wire());
  CHECK_EQ(rig.commands() - released, 3u);
  CHECK_EQ(rig.presenter.framebuffer(), 0u);
  CHECK(rig.presenter.invariants_hold());
  /* Without a wire the host's objects are gone with its context: only the
   * names and the pixels are let go. */
  Rig quiet;
  quiet.host().gl_window(1u, 1u);
  CHECK_STATUS(quiet.presenter.prepare(quiet.wire(), &quiet.window, plain_d24s8, quiet.surface), Status::ok);
  const std::uint32_t kept = quiet.commands();
  quiet.presenter.release(nullptr);
  CHECK_EQ(quiet.commands(), kept);
  CHECK_EQ(quiet.presenter.framebuffer(), 0u);
  CHECK(quiet.presenter.invariants_hold());
  quiet.presenter.release(nullptr); }

TEST(back_buffer_presenter, reports_its_window_names_size_and_format)
{ Rig rig;
  rig.host().gl_window(2u, 2u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, multisampled_d32_srgb, rig.surface), Status::ok);
  char bytes[256];
  av::StateWriter writer(bytes, sizeof bytes);
  rig.presenter.report(writer, 3u);
  CHECK(std::strstr(writer.dump(), "presenter/3 hwnd=0x") == writer.dump());
  CHECK(std::strstr(writer.dump(), " framebuffer=100 resolve=101 width=8 height=4 samples=4 srgb=true") != nullptr); }

TEST(back_buffer_presenter_invariants, fail_when_a_made_presenter_is_missing_a_part)
{
  const auto made = [](Rig &rig, const PixelFormat &format, std::uint32_t framebuffers, std::uint32_t renderbuffers) {
    rig.host().gl_window(framebuffers, renderbuffers);
    CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, format, rig.surface), Status::ok);
  };
  Rig pixels;
  made(pixels, plain_d24s8, 1u, 1u);
  std::uint8_t *const held = Probe::swap_pixels(pixels.presenter, nullptr);
  CHECK(!pixels.presenter.invariants_hold());
  static_cast<void>(Probe::swap_pixels(pixels.presenter, held));
  Rig small;
  made(small, plain_d24s8, 1u, 1u);
  Probe::shrink(small.presenter);
  CHECK(!small.presenter.invariants_hold());
  Rig texture;
  made(texture, plain_d24s8, 1u, 1u);
  Probe::drop_texture(texture.presenter);
  CHECK(!texture.presenter.invariants_hold());
  Rig resolve;
  made(resolve, multisampled_d32_srgb, 2u, 2u);
  Probe::drop_resolve(resolve.presenter);
  CHECK(!resolve.presenter.invariants_hold());
  Rig depth;
  made(depth, plain_d24s8, 1u, 1u);
  Probe::drop_depth(depth.presenter);
  CHECK(!depth.presenter.invariants_hold());
  Rig orphan;
  made(orphan, plain_d24s8, 1u, 1u);
  Probe::drop_framebuffer(orphan.presenter);
  CHECK(!orphan.presenter.invariants_hold());
  /* Unmade, any name left is broken: the resolve framebuffer, the colour
   * renderbuffer, the depth one. */
  Rig names;
  made(names, multisampled_d32_srgb, 2u, 2u);
  Probe::drop_framebuffer(names.presenter);
  CHECK(!names.presenter.invariants_hold());
  Probe::drop_resolve(names.presenter);
  Probe::drop_texture(names.presenter);
  CHECK(!names.presenter.invariants_hold());
  Probe::drop_colour(names.presenter);
  CHECK(!names.presenter.invariants_hold());
  Probe::drop_depth(names.presenter);
  CHECK(names.presenter.invariants_hold());
  Rig colour;
  made(colour, multisampled_d32_srgb, 2u, 2u);
  Probe::drop_colour(colour.presenter);
  CHECK(!colour.presenter.invariants_hold()); }

/* With a present thread the swap does not wait for its frame: it reads it
 * one-way into a slot of room set aside in the arena, hands it over once the
 * batch is sent, and the thread blits what the host wrote there - which is
 * then what GL_FRONT answers. */
TEST(back_buffer_presenter, with_a_present_thread_a_frame_is_read_one_way_and_shown_by_the_thread)
{ Rig      rig;
  Presents presents(rig);
  rig.host().gl_window(1u, 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  const std::size_t lendable = rig.wire().blobs().room();
  const std::size_t frame = 8u * 4u * 4u;
  std::uint32_t     read = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop));
  CHECK_EQ(rig.commands() - read, 1u);
  const IntoBlob first = into_blob(rig, read);
  CHECK_EQ(first.texture, 101u);
  CHECK_EQ(first.width, 8u);
  CHECK_EQ(first.height, 4u);
  CHECK_EQ(first.mode, 3u);
  CHECK_EQ(first.bytes, frame);
  CHECK_EQ(rig.wire().blobs().room(), lendable - 2u * frame);
  CHECK_EQ(first.at, lendable - 2u * frame);
  CHECK_EQ(Probe::room_bytes(rig.presenter), 2u * frame);
  CHECK_EQ(rig.surface.blits(), 0u);
  CHECK_EQ(presents.loop.queued(), 0u);
  CHECK(rig.presenter.invariants_hold());
  rig.context.flush();
  rig.presenter.hand_over(rig.context.mark());
  CHECK_EQ(presents.loop.queued(), 1u);
  /* What the host writes when it runs the read. */
  std::uint8_t pixels[frame];
  pattern(pixels, sizeof pixels);
  std::memcpy(Probe::room(rig.presenter), pixels, sizeof pixels);
  CHECK(presents.thread.step());
  CHECK_EQ(rig.surface.blits(), 1u);
  CHECK(std::memcmp(rig.surface.last_blit_bytes(), pixels, sizeof pixels) == 0);
  std::uint8_t front[frame] = {};
  CHECK(rig.presenter.read_front(0, 0, 8, 4, 0x80E1u, 0x1401u, front));
  CHECK(std::memcmp(front, pixels, sizeof pixels) == 0);
  /* Two frames in the thread's hands at once; a third waits, then is given up. */
  read = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop));
  rig.presenter.hand_over(rig.context.mark());
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop));
  rig.presenter.hand_over(rig.context.mark());
  CHECK_EQ(into_blob(rig, read).at, first.at);
  CHECK_EQ(into_blob(rig, read + 1u).at, first.at + frame);
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop));
  rig.presenter.hand_over(rig.context.mark());
  CHECK_EQ(rig.commands() - read, 2u);
  CHECK_EQ(presents.loop.dropped_frames(), 1u);
  CHECK_EQ(presents.loop.queued(), 2u);
  CHECK(rig.presenter.invariants_hold()); }

/* The room is made again for a window of another size, once the thread has
 * nothing of the old one's; going, the presenter gives its room back. */
TEST(back_buffer_presenter, a_resize_takes_new_room_after_the_thread_and_release_gives_it_back)
{ Rig      rig;
  Presents presents(rig);
  const std::size_t lendable = rig.wire().blobs().room();
  rig.host().gl_window(1u, 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop));
  rig.context.flush();
  rig.presenter.hand_over(rig.context.mark());
  rig.surface.resize(&rig.window, 16u, 8u);
  rig.host().gl_window(1u, 1u, 200u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  /* The thread was never stepped: the frame it did not show was dropped. */
  CHECK_EQ(presents.loop.queued(), 0u);
  CHECK_EQ(presents.loop.dropped_frames(), 1u);
  const std::uint32_t read = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop));
  CHECK_EQ(into_blob(rig, read).bytes, 16u * 8u * 4u);
  CHECK_EQ(Probe::room_bytes(rig.presenter), 2u * 16u * 8u * 4u);
  CHECK_EQ(rig.wire().blobs().room(), lendable - 2u * 16u * 8u * 4u);
  rig.presenter.release(&rig.wire());
  CHECK_EQ(rig.wire().blobs().room(), lendable);
  CHECK_EQ(Probe::room_bytes(rig.presenter), 0u);
  CHECK(rig.presenter.invariants_hold()); }

/* No arena to set room aside in, room that will not fit, or no thread: the
 * swap reads back and blits the frame itself, after whatever the thread
 * still had of this window's. A pack state that moves rows is set aside for
 * a read into a slot as for any other. */
TEST(back_buffer_presenter, without_room_or_a_thread_the_swap_shows_the_frame_itself)
{ { Rig      rig(false);
    Presents presents(rig);
    rig.host().gl_window(1u, 1u);
    CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
    rig.host().gl_bytes(nullptr, 0u);
    CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop));
    CHECK_EQ(rig.surface.blits(), 1u);
    CHECK_EQ(presents.thread.starts(), 1u); }
  { Rig      rig;
    Presents presents(rig);
    rig.wire().blobs().set_aside(rig.wire().blobs().room() - 64u);
    rig.host().gl_window(1u, 1u);
    CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
    rig.host().gl_bytes(nullptr, 0u);
    CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop));
    CHECK_EQ(rig.surface.blits(), 1u);
    CHECK_EQ(Probe::room_bytes(rig.presenter), 0u); }
  { Rig      rig;
    Presents presents(rig);
    presents.loop.refuse();
    rig.host().gl_window(1u, 1u);
    CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
    rig.host().gl_bytes(nullptr, 0u);
    CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop));
    CHECK_EQ(rig.surface.blits(), 1u); }
  Rig      rig;
  Presents presents(rig);
  rig.host().gl_window(1u, 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  rig.state.bind_buffer(ClientArrayState::pixel_pack_buffer, 9u);
  const std::uint32_t read = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop));
  CHECK_EQ(rig.commands() - read, 15u);
  CHECK_EQ(into_blob(rig, read + 7u).mode, 3u);
  CHECK_EQ(rig.argument(read + 8u, 1u), 9u); }

/* Nothing read for the thread, nothing handed; a frame shown bigger than the
 * pixels the front buffer keeps is not kept. */
TEST(back_buffer_presenter, a_hand_over_with_no_frame_read_hands_nothing)
{ Rig      rig;
  Presents presents(rig);
  rig.presenter.hand_over(rig.context.mark());
  rig.host().gl_window(1u, 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  rig.presenter.hand_over(rig.context.mark());
  CHECK_EQ(presents.loop.queued(), 0u);
  std::uint8_t pixels[16u * 8u * 4u] = {};
  rig.presenter.shown(pixels, 16u, 8u);
  CHECK(!rig.presenter.front().presented());
  rig.presenter.shown(pixels, 8u, 4u);
  CHECK(rig.presenter.front().presented()); }

TEST(back_buffer_presenter_invariants, fail_for_a_frame_pending_without_room_or_room_without_its_arena)
{ Rig      rig;
  Presents presents(rig);
  rig.host().gl_window(1u, 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  Probe::set_pending(rig.presenter, Presenter::slot_count + 1u);
  CHECK(!rig.presenter.invariants_hold());
  Probe::set_pending(rig.presenter, 0u);
  CHECK(!rig.presenter.invariants_hold());
  Probe::set_pending(rig.presenter, Presenter::slot_count);
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop));
  CHECK(rig.presenter.invariants_hold());
  Probe::drop_arena(rig.presenter);
  CHECK(!rig.presenter.invariants_hold()); }
