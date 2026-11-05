#include "core/log/driver_log.hpp"
#include "executor/executor_log.hpp"
#include "executor/exported_memory.hpp"
#include "executor/owned_fd.hpp"
#include "executor/source_memory.hpp"
#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "executor/opengl/opengl_backend.hpp"
#include "executor/shim/gl_display.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "executor/sharing/shared_image_shape.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/submission/blob_role.hpp"
#include "proto/wire/guest_bytes.hpp"
#include "proto/generated/opengl/gl_dispatch_server.h"
#include "proto/generated/opengl/gl_protocol_ids.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

namespace av {
struct OpenGlBackendProbe {
    static glproto::RealGl &gl(OpenGlBackend &backend) { return backend.gl_; }
    static bool granted_fixed_function(OpenGlBackend &backend) { return backend.granted_fixed_function(); }
    static void made_current(OpenGlBackend &backend) { backend.current_ = true; }
    static void install_hooks(OpenGlBackend &backend) { backend.install_hooks(); }
    static std::uint32_t live_syncs(OpenGlBackend &backend) { return backend.fences_.count(); }
    static const GuestBytes &guest(OpenGlBackend &backend) { return backend.guest_; }
};

}

using av::OpenGlBackend;
using av::OpenGlBackendProbe;
using av::WireArena;
using av::WireReader;
using av::WireWriter;

extern "C" const char *__asan_default_options(void);
extern "C" const char *__asan_default_options(void)
{
    return "detect_leaks=0";
}

extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libOpenCL\n"
           "leak:intelocl\n"
           "leak:libigdrcl\n"
           "leak:libvulkan\n"
           "leak:libEGL\n"
           "leak:libGLX\n"
           "leak:libnvidia\n"
           "leak:iris_dri\n"
           "leak:libdbus-1\n"
           "leak:libglapi\n"
           "leak:libz\n";
}

namespace {
std::uint64_t detail = 0u;

struct Lines {
    Lines() : memory(), sink(), executor(sink), log(executor.opengl()) {}

    av::MockPlatform memory;
    av::RecordingLog sink;
    av::ExecutorLog  executor;
    av::DriverLog   &log;
};

av::BlobRole arena_at(std::uint32_t slot)
{
    return av::BlobRole(av::BlobRole::Kind::arena_segment, std::uint64_t{slot} << av::GuestBytes::slot_shift);
}

std::uint64_t wire_of(const void *object)
{
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(object));
}

alignas(4096) std::uint8_t region[2u * 4096u];
alignas(4096) std::uint8_t second_region[4096];

av::SourceMemory &memory_over(std::uint8_t *base, std::uint64_t bytes)
{
    static av::SourceMemory whole(1u, 1u, 0u, AV_MEMORY_WRITE_BACK,
                                  av::ExportedMemory(1u, -1, region, sizeof region));
    static av::SourceMemory other(1u, 2u, 0u, AV_MEMORY_WRITE_BACK,
                                  av::ExportedMemory(2u, -1, second_region, sizeof second_region));
    return base == region && bytes == sizeof region ? whole : other;
}
constexpr GLenum  gl_context_profile_mask = 0x9126u, gl_invalid_enum = 0x0500u;
constexpr GLint   core_profile_bit = 0x1, compatibility_profile_bit = 0x2;
constexpr std::uint32_t endless = 1000u;
GLint             answered_mask = 0;
std::uint32_t     errors_left = 0u;
std::uint32_t     errors_asked = 0u;

void profile_integers(GLenum pname, GLint *value)
{
    if (pname == gl_context_profile_mask) *value = answered_mask;
}

GLenum pending_error()
{
    errors_asked++;
    if (errors_left == 0u) return 0u;
    errors_left--;
    return gl_invalid_enum;
}

bool fixed_function_for(GLint mask, std::uint32_t errors)
{
    Lines backend_lines;
    OpenGlBackend backend(backend_lines.log);
    av::glproto::RealGl &gl = OpenGlBackendProbe::gl(backend);
    gl.glGetIntegerv = profile_integers;
    gl.glGetError = pending_error;
    answered_mask = mask;
    errors_left = errors;
    errors_asked = 0u;
    return OpenGlBackendProbe::granted_fixed_function(backend);
}

constexpr std::uint8_t no_such_gpu[16] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

}

TEST(opengl_backend, knows_the_ids_the_profile_carries_and_no_others)
{ Lines backend_lines;
  OpenGlBackend backend(backend_lines.log);
  CHECK(backend.knows(av::glproto::kCmd_glClear));
  CHECK(!backend.knows(0x3FFFFu));
  CHECK(!backend.knows(0x2FFFFu));
  CHECK(!backend.knows(0u)); }

TEST(opengl_backend, without_a_context_no_fence_is_bound_and_the_sync_is_left_to_the_caller)
{ Lines backend_lines;
  OpenGlBackend backend(backend_lines.log);
  int sync = 0;
  CHECK(!backend.bind_fence(7u, &sync));
  CHECK(backend.fence_for(7u) == nullptr);
  CHECK(backend.forget_fence(7u) == nullptr);
  CHECK_EQ(OpenGlBackendProbe::live_syncs(backend), 0u);
  backend.shut_down(); }

TEST(opengl_backend, guest_bytes_answers_inside_the_region_and_nowhere_else)
{ Lines backend_lines;
  OpenGlBackend backend(backend_lines.log);
  const av::GuestBytes &guest = OpenGlBackendProbe::guest(backend);
  CHECK(guest.at(0u, 1u) == nullptr);
  std::uint64_t handle = 7u;
  av::SourceMemory &memory = memory_over(region, sizeof region);
  CHECK(backend.adopt(memory, arena_at(0u), handle));
  CHECK_EQ(handle, wire_of(region));
  CHECK(guest.at(0u, sizeof region) == region);
  CHECK(guest.at(16u, 32u) == region + 16u);
  CHECK(guest.at(0u, sizeof region + 1u) == nullptr);
  CHECK(guest.at(~std::uint64_t{0}, 1u) == nullptr);
  backend.unbind_arena(memory);
  CHECK(guest.at(0u, 1u) == nullptr);
  CHECK(guest.invariants_hold()); }

TEST(opengl_backend, a_gpu_with_no_display_opens_no_context)
{ Lines backend_lines;
  OpenGlBackend backend(backend_lines.log);
  CHECK(!backend.initialize(nullptr, nullptr, nullptr));
  av::GlDisplay display;
  CHECK(!display.open(no_such_gpu));
  CHECK(!display.open());
  CHECK(!display.open(nullptr));
  CHECK(!backend.initialize(&display, nullptr, nullptr));
  CHECK_EQ(backend.resolved(), 0u); }

TEST(opengl_backend, only_a_core_context_is_told_it_has_no_fixed_function_arrays)
{ CHECK(!fixed_function_for(core_profile_bit, 0u));
  CHECK_EQ(errors_asked, 1u);
  CHECK(fixed_function_for(compatibility_profile_bit, 0u));
  CHECK(fixed_function_for(0, 1u));
  CHECK_EQ(errors_left, 0u);
  CHECK_EQ(errors_asked, 2u);
  CHECK(!fixed_function_for(core_profile_bit | compatibility_profile_bit, endless));
  CHECK_EQ(errors_asked, 8u);
  Lines unloaded_lines;
  OpenGlBackend unloaded(unloaded_lines.log);
  CHECK(OpenGlBackendProbe::granted_fixed_function(unloaded));
  OpenGlBackendProbe::gl(unloaded).glGetIntegerv = profile_integers;
  CHECK(OpenGlBackendProbe::granted_fixed_function(unloaded)); }

TEST(opengl_backend, without_a_context_nothing_runs)
{ Lines backend_lines;
  OpenGlBackend backend(backend_lines.log);
  std::uint8_t block[4096];
  WireArena arena(block, sizeof block);
  std::uint8_t frame[16] = {};
  std::uint8_t reply[16] = {};
  WireReader payload(frame, sizeof frame);
  WireWriter answer(reply, sizeof reply);
  CHECK(!backend.run(av::glproto::kCmd_glClear, payload, answer, arena));
  CHECK_EQ(backend.ran(), 0u);
  CHECK(!backend.lost());
  backend.shut_down();
  CHECK_EQ(backend.resolved(), 0u); }

TEST(opengl_backend, adopts_arena_segments_at_their_slots_and_lends_a_plain_blob_only_with_a_context)
{ Lines backend_lines;
  OpenGlBackend backend(backend_lines.log);
  const av::GuestBytes &guest = OpenGlBackendProbe::guest(backend);
  av::SourceMemory &whole = memory_over(region, sizeof region);
  av::SourceMemory &other = memory_over(second_region, sizeof second_region);
  std::uint64_t handle = 7u;
  CHECK(backend.adopt(whole, arena_at(0u), handle));
  CHECK_EQ(handle, wire_of(region));
  CHECK(guest.at(4096u, 4096u) == region + 4096u);
  CHECK(guest.at(4096u, 4097u) == nullptr);
  std::uint64_t second = 7u;
  CHECK(!backend.adopt(other, arena_at(0u), second));
  CHECK_EQ(second, 0u);
  CHECK(!backend.adopt(other, av::BlobRole(), second));
  CHECK_EQ(second, 0u);
  CHECK(guest.at(0u, 1u) == region);
  CHECK(backend.adopt(other, arena_at(1u), second));
  CHECK_EQ(second, wire_of(second_region));
  CHECK(guest.at(av::GuestBytes::slot_bytes, 4096u) == second_region);
  CHECK(guest.at(0u, 1u) == region);
  backend.unbind_arena(whole);
  CHECK(guest.at(0u, 1u) == nullptr);
  CHECK(guest.at(av::GuestBytes::slot_bytes, 1u) == second_region);
  backend.unbind_arena(other);
  CHECK(guest.at(av::GuestBytes::slot_bytes, 1u) == nullptr);
  CHECK(guest.invariants_hold());
  backend.shut_down();
  av::SourceMemory unmapped(1u, 3u, 0u, AV_MEMORY_WRITE_BACK, av::ExportedMemory(3u, -1, nullptr, 4096u));
  handle = 7u;
  CHECK(!backend.adopt(unmapped, arena_at(0u), handle));
  CHECK_EQ(handle, 0u);
  CHECK(guest.at(0u, 1u) == nullptr); }

TEST(opengl_backend, a_context_on_a_real_gpu_runs_a_command_and_refuses_a_broken_one)
{ Lines host_lines;
  av::LoaderHostApis host(host_lines.executor);
  host.open();
  const av::GlDisplay *display = nullptr;
  for (std::uint32_t i = 0u; i < host.gpu_count() && display == nullptr; i++) {
      display = host.gl_display(i);
      CHECK_EQ(display != nullptr, (host.sub_apis(i) & AV_EXECUTOR_SUB_API_OPENGL) != 0u);
  }
  CHECK(host.gl_display(host.gpu_count()) == nullptr);
  if (display == nullptr) return;

  Lines backend_lines;
  OpenGlBackend backend(backend_lines.log);
  CHECK(backend.initialize(display, nullptr, nullptr));
  CHECK(backend.resolved() > 0u);
  Lines other_lines;
  OpenGlBackend other(other_lines.log);
  CHECK(other.initialize(display, nullptr, nullptr));

  std::uint8_t block[4096];
  WireArena arena(block, sizeof block);
  std::uint8_t frame[16];
  WireWriter clear(frame, sizeof frame);
  clear.write_varint<std::uint32_t>(0x4000u);
  CHECK_EQ(clear.offset(), 3u);
  std::uint8_t reply[16] = {};
  WireReader payload(frame, clear.offset());
  WireWriter answer(reply, sizeof reply);
  CHECK(backend.run(av::glproto::kCmd_glClear, payload, answer, arena));
  CHECK_EQ(backend.ran(), 1u);

  arena.reset();
  WireReader short_payload(frame, 1u);
  WireWriter short_answer(reply, sizeof reply);
  CHECK(!backend.run(av::glproto::kCmd_glClear, short_payload, short_answer, arena));
  CHECK_EQ(backend.ran(), 1u);

  backend.shut_down();
  CHECK_EQ(backend.resolved(), 0u);
  arena.reset();
  WireReader again(frame, clear.offset());
  WireWriter again_answer(reply, sizeof reply);
  CHECK(!backend.run(av::glproto::kCmd_glClear, again, again_answer, arena));
  arena.reset();
  WireReader still(frame, clear.offset());
  WireWriter still_answer(reply, sizeof reply);
  CHECK(other.run(av::glproto::kCmd_glClear, still, still_answer, arena));
  CHECK(display->open());
  other.shut_down();
  host.close();
  CHECK(!display->open()); }

TEST(opengl_backend, a_backend_with_no_context_exports_imports_and_releases_nothing)
{ Lines backend_lines;
  OpenGlBackend backend(backend_lines.log);
  std::uint32_t handle_kind = 9u;
  av::OwnedFd   descriptor;
  CHECK(!backend.export_object(AV_SHARED_MEMORY, 1u, handle_kind, descriptor));
  CHECK_EQ(handle_kind, 0u);
  CHECK(!descriptor.held());
  std::uint64_t made = 9u;
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, 0, 4096u, av::SharedImageShape(), 0u, made, detail));
  CHECK_EQ(made, 0u);
  backend.release_object(AV_SHARED_MEMORY, 1u, detail);
  backend.shut_down(); }

TEST(opengl_backend, an_import_of_the_wrong_shape_never_reaches_a_driver)
{ Lines backend_lines;
  OpenGlBackend backend(backend_lines.log);
  std::uint64_t made = 9u;
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_OPAQUE_FD, 0, 4096u, av::SharedImageShape(), 0u, made, detail));
  CHECK_EQ(made, 0u);
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, -1, 4096u, av::SharedImageShape(), 0u, made, detail));
  CHECK(!backend.import_object(AV_SHARED_NATIVE_FENCE + 1u, AV_SHARED_HANDLE_DMA_BUF, 0, 4096u, av::SharedImageShape(), 0u, made, detail));
  CHECK(!backend.import_object(0u, AV_SHARED_HANDLE_DMA_BUF, 0, 4096u, av::SharedImageShape(), 0u, made, detail));
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, 0, 0u, av::SharedImageShape(), 0u, made, detail));
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, 0, 4096u, av::SharedImageShape(), 1u, made, detail));
  CHECK_EQ(made, 0u); }

namespace {

void never_called(void *) {}

}

TEST(opengl_backend, a_signal_changes_nothing_it_waits_on)
{ Lines backend_lines;
  OpenGlBackend backend(backend_lines.log);
  backend.bind_signal(&never_called, nullptr);
  CHECK(!backend.poll());
  std::uint64_t marked = 7u, reached = 7u;
  backend.gpu_progress(marked, reached);
  CHECK_EQ(marked, 0u);
  CHECK_EQ(reached, 0u); }
