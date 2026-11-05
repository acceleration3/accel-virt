#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/counted_wake_signal.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_thread.hpp"
#include "harness/icd/scripted_window_chains.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "icd/opengl/back_buffer_presenter.hpp"
#include "icd/opengl/client_array_state.hpp"
#include "icd/common/window_present_queue.hpp"
#include "icd/opengl/batch_ready.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "icd/opengl/pixel_format.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/shared_object_answer.hpp"
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

}

namespace {

constexpr std::uint32_t gl_rgba8 = 0x8058u;
constexpr std::uint32_t gl_srgb8_alpha8 = 0x8C43u;

const PixelFormat plain_d24s8(24u, 8u, true, false, 0u);
const PixelFormat multisampled_d32_srgb(32u, 0u, false, true, 4u);
const PixelFormat bare(0u, 0u, true, false, 0u);

struct Rig {
    GlContextRig               context_rig;
    GuestGlContext             context{context_rig.connection(), context_rig.platform(), context_rig.send_lock(),
                                       context_rig.groups(), context_rig.lines(), context_rig.extensions(),
                                       context_rig.handles(), 1, GlContextOptions()};
    av::ScriptedWindowSurface  surface;
    ClientArrayState           state{context_rig.platform()};
    int                        dc = 0;
    int                        window = 0;
    Presenter                  presenter{context_rig.platform(), context_rig.lines()};

    explicit Rig(bool arena = true, std::uint32_t width = 8u, std::uint32_t height = 4u)
    {
        if (!arena) {
            host().blob_import(1u, 0u);
            host().blob_import(1u, 0u);
            host().gl_string("vendor");
            host().gl_string("renderer");
            host().gl_string("4.6");
            host().gl_string("4.60");
            for (int list = 0; list < 3; list++) host().gl_integer(0);
            host().gl_integer(4);
            host().gl_integer(6);
            host().gl_integer(2);
            host().gl_integer(0);
        } else {
            host().gl_made("renderer", "4.6");
        }
        CHECK_STATUS(context.make(), Status::ok);
        surface.add_dc(&dc, &window, width, height);
    }
    av::ScriptedHost    &host() { return context_rig.host(); }
    av::MockKmt         &kmt() { return context_rig.kmt(); }
    av::ClientTransport &wire() { return *context.wire(); }
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

struct Presents {
    av::ScriptedThread       thread;
    av::CountedWakeSignal    handed;
    av::CountedWakeSignal    shown;
    av::LockTrace            trace;
    av::RecordingThreadLock  lock{trace, 'P'};
    av::ScriptedWindowChains chains;
    av::glicd::BatchReady    ready;
    av::glicd::GlPresentQueue loop;

    explicit Presents(Rig &rig)
        : ready(rig.kmt()),
          loop(rig.context_rig.platform(), thread, handed, shown, lock, ready, rig.surface, chains,
               rig.context_rig.lines()),
          rig_(rig)
    {}
    ~Presents() { rig_.presenter.release(nullptr); }

private:
    Rig &rig_;
};

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

}

TEST(back_buffer_presenter, a_first_frame_makes_the_framebuffer_and_puts_the_hosts_pixels_on_the_window)
{ Rig rig;
  CHECK(rig.presenter.invariants_hold());
  CHECK(!rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, nullptr, 0u));
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
  CHECK_EQ(rig.argument(made + 3u, 0u), 101u);
  CHECK_EQ(rig.argument(made + 3u, 2u), gl_rgba8);
  CHECK_EQ(rig.argument(made + 3u, 3u), 8u);
  CHECK_EQ(rig.argument(made + 3u, 4u), 4u);
  CHECK_EQ(rig.argument(made + 4u, 0u), 100u);
  CHECK_EQ(rig.argument(made + 4u, 1u), Presenter::gl_color_attachment0);
  CHECK_EQ(rig.argument(made + 5u, 0u), 102u);
  CHECK_EQ(rig.argument(made + 5u, 1u), 0u);
  CHECK_EQ(rig.argument(made + 5u, 2u), Presenter::gl_depth24_stencil8);
  CHECK_EQ(rig.argument(made + 6u, 1u), Presenter::gl_depth_stencil_attachment);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  CHECK_EQ(rig.commands() - made, 7u);
  std::uint8_t frame[8u * 4u * 4u];
  pattern(frame, sizeof frame);
  rig.host().gl_bytes(frame, sizeof frame);
  const std::uint32_t read = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, nullptr, 0u));
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
  const std::size_t live = rig.context_rig.platform().live_allocations();
  rig.surface.resize(&rig.window, 2u, 2u);
  rig.host().gl_window(1u, 1u, 300u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  CHECK_EQ(rig.context_rig.platform().live_allocations(), live);
  CHECK_EQ(rig.presenter.framebuffer(), 300u);
  int other = 0;
  int other_dc = 0;
  rig.surface.add_dc(&other_dc, &other, 2u, 2u);
  rig.host().gl_window(1u, 1u, 400u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &other, plain_d24s8, rig.surface), Status::ok);
  CHECK_EQ(rig.presenter.framebuffer(), 400u);
  CHECK(rig.presenter.window() == &other);
  rig.surface.resize(&other, 2u, 3u);
  rig.host().gl_window(1u, 1u, 500u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &other, plain_d24s8, rig.surface), Status::ok);
  CHECK_EQ(rig.presenter.framebuffer(), 500u);
  CHECK_EQ(rig.presenter.height(), 3u); }

TEST(back_buffer_presenter, a_multisampled_frame_is_resolved_before_it_is_read)
{ Rig rig;
  rig.host().gl_window(2u, 2u);
  const std::uint32_t made = rig.commands();
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, multisampled_d32_srgb, rig.surface), Status::ok);
  CHECK(rig.presenter.invariants_hold());
  CHECK(!rig.presenter.double_buffered());
  CHECK_EQ(rig.argument(made, 0u), 2u);
  CHECK_EQ(rig.argument(made + 2u, 0u), 2u);
  CHECK_EQ(rig.id(made + 3u), ids::kCmd_glTextureStorage2D);
  CHECK_EQ(rig.id(made + 4u), ids::kCmd_glNamedFramebufferTexture);
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
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, nullptr, 0u));
  CHECK_EQ(rig.id(read), ids::kCmd_glBlitNamedFramebuffer);
  CHECK_EQ(rig.argument(read, 0u), 100u);
  CHECK_EQ(rig.argument(read, 1u), 101u);
  CHECK_EQ(rig.id(read + 1u), ids::kCmd_glGetTextureSubImage);
  CHECK_EQ(rig.surface.blits(), 1u); }

TEST(back_buffer_presenter, an_srgb_format_stores_srgb_colour)
{ Rig rig;
  rig.host().gl_window(2u, 2u);
  const std::uint32_t made = rig.commands();
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, multisampled_d32_srgb, rig.surface), Status::ok);
  CHECK_EQ(rig.argument(made + 3u, 2u), gl_srgb8_alpha8);
  CHECK_EQ(rig.argument(made + 5u, 2u), gl_srgb8_alpha8); }

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
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, nullptr, 0u));
  CHECK_EQ(rig.commands(), swapped);
  CHECK_EQ(rig.surface.blits(), 0u);
  Rig wide(true, 3u, 0u);
  wide.host().gl_window(1u, 1u);
  CHECK_STATUS(wide.presenter.prepare(wide.wire(), &wide.window, plain_d24s8, wide.surface), Status::ok);
  CHECK(wide.presenter.present(wide.wire(), wide.state, &wide.dc, wide.surface, nullptr, 0u));
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

TEST(back_buffer_presenter, the_applications_pack_state_is_set_aside_for_the_read)
{ Rig rig(true, 3u, 2u);
  rig.host().gl_window(1u, 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  rig.state.bind_buffer(ClientArrayState::pixel_pack_buffer, 9u);
  CHECK(rig.state.store_pixel(Presenter::gl_pack_skip_rows, 2));
  rig.host().gl_bytes(nullptr, 0u);
  std::uint32_t read = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, nullptr, 0u));
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
  rig.state.bind_buffer(ClientArrayState::pixel_pack_buffer, 0u);
  CHECK(rig.state.store_pixel(Presenter::gl_pack_skip_rows, 0));
  CHECK(rig.state.store_pixel(Presenter::gl_pack_alignment, 8));
  rig.host().gl_bytes(nullptr, 0u);
  read = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, nullptr, 0u));
  CHECK_EQ(rig.commands() - read, 15u);
  CHECK_EQ(rig.argument(read + 9u, 1u), 8u);
  CHECK(rig.state.store_pixel(Presenter::gl_pack_alignment, 1));
  rig.host().gl_bytes(nullptr, 0u);
  read = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, nullptr, 0u));
  CHECK_EQ(rig.commands() - read, 1u); }

TEST(back_buffer_presenter, a_tall_frame_is_read_in_rows_a_reply_holds)
{ Rig rig(false, 64u, 300u);
  const std::uint32_t row_bytes = 64u * 4u;
  const std::uint32_t rows = static_cast<std::uint32_t>(rig.wire().room().inline_read()) / row_bytes;
  CHECK(rows > 0u && rows < 300u);
  rig.host().gl_window(1u, 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  rig.host().gl_bytes(nullptr, 0u);
  rig.host().gl_bytes(nullptr, 0u);
  const std::uint32_t read = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, nullptr, 0u));
  CHECK_EQ(rig.commands() - read, 3u);
  CHECK_EQ(rig.id(read), ids::kCmd_glGetTextureSubImage);
  CHECK(rig.id(read + 1u) != ids::kCmd_glGetTextureSubImage);
  CHECK_EQ(rig.id(read + 2u), ids::kCmd_glGetTextureSubImage);
  CHECK_EQ(rig.argument(read, 3u), 0u);
  CHECK_EQ(rig.argument(read, 6u), rows);
  CHECK_EQ(rig.argument(read + 2u, 3u), rows);
  CHECK_EQ(rig.argument(read + 2u, 6u), 300u - rows); }

TEST(back_buffer_presenter, a_refused_blit_is_logged_once_and_the_swap_happened)
{ Rig rig;
  rig.host().gl_window(1u, 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  for (int frame = 0; frame < 2; frame++) {
    rig.surface.refuse_next_blit();
    rig.host().gl_bytes(nullptr, 0u);
    CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, nullptr, 0u));
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

TEST(back_buffer_presenter, with_a_present_thread_a_frame_is_read_one_way_and_shown_by_the_thread)
{ Rig      rig;
  Presents presents(rig);
  rig.host().gl_window(1u, 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  const std::size_t lendable = rig.wire().blobs().room();
  const std::size_t frame = 8u * 4u * 4u;
  std::uint32_t     read = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop, 0u));
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
  std::uint8_t pixels[frame];
  pattern(pixels, sizeof pixels);
  std::memcpy(Probe::room(rig.presenter), pixels, sizeof pixels);
  CHECK(presents.thread.step());
  CHECK_EQ(rig.surface.blits(), 1u);
  CHECK(std::memcmp(rig.surface.last_blit_bytes(), pixels, sizeof pixels) == 0);
  std::uint8_t front[frame] = {};
  CHECK(rig.presenter.read_front(0, 0, 8, 4, 0x80E1u, 0x1401u, front));
  CHECK(std::memcmp(front, pixels, sizeof pixels) == 0);
  read = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop, 0u));
  rig.presenter.hand_over(rig.context.mark());
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop, 0u));
  rig.presenter.hand_over(rig.context.mark());
  CHECK_EQ(into_blob(rig, read).at, first.at);
  CHECK_EQ(into_blob(rig, read + 1u).at, first.at + frame);
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop, 0u));
  rig.presenter.hand_over(rig.context.mark());
  CHECK_EQ(rig.commands() - read, 2u);
  CHECK_EQ(presents.loop.dropped_frames(), 1u);
  CHECK_EQ(presents.loop.queued(), 2u);
  CHECK(rig.presenter.invariants_hold()); }

TEST(back_buffer_presenter, a_resize_takes_new_room_after_the_thread_and_release_gives_it_back)
{ Rig      rig;
  Presents presents(rig);
  const std::size_t lendable = rig.wire().blobs().room();
  rig.host().gl_window(1u, 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop, 0u));
  rig.context.flush();
  rig.presenter.hand_over(rig.context.mark());
  rig.surface.resize(&rig.window, 16u, 8u);
  rig.host().gl_window(1u, 1u, 200u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  CHECK_EQ(presents.loop.queued(), 0u);
  CHECK_EQ(presents.loop.dropped_frames(), 1u);
  const std::uint32_t read = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop, 0u));
  CHECK_EQ(into_blob(rig, read).bytes, 16u * 8u * 4u);
  CHECK_EQ(Probe::room_bytes(rig.presenter), 2u * 16u * 8u * 4u);
  CHECK_EQ(rig.wire().blobs().room(), lendable - 2u * 16u * 8u * 4u);
  rig.presenter.release(&rig.wire());
  CHECK_EQ(rig.wire().blobs().room(), lendable);
  CHECK_EQ(Probe::room_bytes(rig.presenter), 0u);
  CHECK(rig.presenter.invariants_hold()); }

TEST(back_buffer_presenter, without_room_or_a_thread_the_swap_shows_the_frame_itself)
{ { Rig      rig(false);
    Presents presents(rig);
    rig.host().gl_window(1u, 1u);
    CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
    rig.host().gl_bytes(nullptr, 0u);
    CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop, 0u));
    CHECK_EQ(rig.surface.blits(), 1u);
    CHECK_EQ(presents.thread.starts(), 1u); }
  { Rig      rig;
    Presents presents(rig);
    rig.wire().blobs().set_aside(rig.wire().blobs().room() - 64u);
    rig.host().gl_window(1u, 1u);
    CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
    rig.host().gl_bytes(nullptr, 0u);
    CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop, 0u));
    CHECK_EQ(rig.surface.blits(), 1u);
    CHECK_EQ(Probe::room_bytes(rig.presenter), 0u); }
  { Rig      rig;
    Presents presents(rig);
    presents.loop.refuse();
    rig.host().gl_window(1u, 1u);
    CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
    rig.host().gl_bytes(nullptr, 0u);
    CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop, 0u));
    CHECK_EQ(rig.surface.blits(), 1u); }
  Rig      rig;
  Presents presents(rig);
  rig.host().gl_window(1u, 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  rig.state.bind_buffer(ClientArrayState::pixel_pack_buffer, 9u);
  const std::uint32_t read = rig.commands();
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop, 0u));
  CHECK_EQ(rig.commands() - read, 15u);
  CHECK_EQ(into_blob(rig, read + 7u).mode, 3u);
  CHECK_EQ(rig.argument(read + 8u, 1u), 9u); }

TEST(back_buffer_presenter, a_hand_over_with_no_frame_read_hands_nothing)
{ Rig      rig;
  Presents presents(rig);
  rig.presenter.hand_over(rig.context.mark());
  rig.host().gl_window(1u, 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  rig.presenter.hand_over(rig.context.mark());
  CHECK_EQ(presents.loop.queued(), 0u);
  std::uint8_t pixels[16u * 8u * 4u] = {};
  const av::BatchMark mark = rig.context.mark();
  using Frame = av::glicd::GlFrame;
  rig.presenter.done(Frame(rig.presenter, 0u, pixels, Frame::Rows::bgra_bottom_up, 16u, 8u, &rig.window, mark), true);
  CHECK(!rig.presenter.front().presented());
  rig.presenter.done(Frame(rig.presenter, 0u, pixels, Frame::Rows::bgra_bottom_up, 8u, 4u, &rig.window, mark), false);
  CHECK(!rig.presenter.front().presented());
  rig.presenter.done(Frame(rig.presenter, 0u, nullptr, Frame::Rows::bgra_bottom_up, 8u, 4u, &rig.window, mark), true);
  CHECK(!rig.presenter.front().presented());
  rig.presenter.done(Frame(rig.presenter, 0u, pixels, Frame::Rows::bgra_bottom_up, 8u, 4u, &rig.window, mark), true);
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
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop, 0u));
  CHECK(rig.presenter.invariants_hold());
  Probe::drop_arena(rig.presenter);
  CHECK(!rig.presenter.invariants_hold()); }

TEST(back_buffer_presenter, with_a_chain_a_frame_is_drawn_into_a_slot_and_presented_by_the_thread)
{ Rig      rig;
  Presents presents(rig);
  presents.chains.refuse_opens(false);
  rig.host().gl_window(1u, 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  const std::size_t lendable = rig.wire().blobs().room();
  rig.host().shared_object(av::SharedObjectAnswer::done, 0x700u);
  rig.host().shared_object(av::SharedObjectAnswer::done, 0x701u);
  const std::uint32_t textures[2] = {200u, 201u};
  rig.host().gl_bytes(reinterpret_cast<const std::uint8_t *>(textures), sizeof textures);
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop, 0x1234u));
  CHECK_EQ(presents.chains.opens(), 1u);
  CHECK_EQ(presents.chains.last_luid(), 0x1234u);
  CHECK_EQ(presents.chains.last_width(), 8u);
  CHECK_EQ(presents.chains.last_height(), 4u);
  CHECK_EQ(rig.wire().blobs().room(), lendable);
  const std::uint32_t sent = rig.commands();
  CHECK_EQ(rig.id(sent - 2u), ids::kCmd_glCopyImageSubData);
  CHECK_EQ(rig.argument(sent - 2u, 0u), 101u);
  CHECK_EQ(rig.argument(sent - 2u, 6u), 200u);
  CHECK_EQ(rig.id(sent - 1u), ids::kCmd_glFinish);
  CHECK(rig.presenter.invariants_hold());
  rig.presenter.hand_over(rig.context.mark());
  CHECK_EQ(presents.loop.queued(), 1u);
  CHECK(presents.thread.step());
  CHECK_EQ(presents.chains.presents(), 1u);
  CHECK_EQ(presents.chains.last_slot(), 0u);
  CHECK_EQ(rig.surface.blits(), 0u);
  std::uint8_t front[8u * 4u * 4u] = {};
  CHECK(!rig.presenter.read_front(0, 0, 8, 4, 0x80E1u, 0x1401u, front));

  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop, 0x1234u));
  CHECK_EQ(rig.argument(rig.commands() - 2u, 6u), 200u);
  rig.presenter.hand_over(rig.context.mark());
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop, 0x1234u));
  CHECK_EQ(rig.argument(rig.commands() - 2u, 6u), 201u);
  rig.presenter.hand_over(rig.context.mark());
  CHECK(presents.thread.step());
  CHECK(presents.thread.step());
  CHECK_EQ(presents.chains.presents(), 3u);
  CHECK_EQ(presents.chains.last_slot(), 1u);
  CHECK_EQ(presents.chains.opens(), 1u);
  rig.presenter.release(&rig.wire());
  CHECK_EQ(presents.chains.closes(), 1u);
  CHECK(presents.chains.last_closed() == &rig.window); }

TEST(back_buffer_presenter, a_refused_chain_reads_frames_back_until_the_window_resizes)
{ Rig      rig;
  Presents presents(rig);
  rig.host().gl_window(1u, 1u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop, 0x1234u));
  CHECK_EQ(presents.chains.opens(), 1u);
  CHECK_EQ(into_blob(rig, rig.commands() - 1u).width, 8u);
  rig.presenter.hand_over(rig.context.mark());
  CHECK(presents.thread.step());
  CHECK_EQ(rig.surface.blits(), 1u);
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop, 0x1234u));
  rig.presenter.hand_over(rig.context.mark());
  CHECK_EQ(presents.chains.opens(), 1u);
  CHECK(presents.thread.step());
  rig.surface.resize(&rig.window, 16u, 8u);
  rig.host().gl_window(1u, 1u, 300u);
  CHECK_STATUS(rig.presenter.prepare(rig.wire(), &rig.window, plain_d24s8, rig.surface), Status::ok);
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop, 0x1234u));
  CHECK_EQ(presents.chains.opens(), 2u);
  rig.presenter.hand_over(rig.context.mark());
  CHECK(presents.thread.step());
  CHECK(rig.presenter.present(rig.wire(), rig.state, &rig.dc, rig.surface, &presents.loop, 0u));
  CHECK_EQ(presents.chains.opens(), 2u); }
