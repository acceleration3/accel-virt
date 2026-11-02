#include "executor/exported_memory.hpp"
#include "executor/imported_blob.hpp"
#include "executor/source_memory.hpp"
#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "executor/opengl/opengl_backend.hpp"
#include "executor/shim/gl_display.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "executor/sharing/shared_image_shape.hpp"
#include "harness/framework/test_registry.hpp"
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
};

}

using av::OpenGlBackend;
using av::OpenGlBackendProbe;
using av::WireArena;
using av::WireReader;
using av::WireWriter;

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
int          fence = 0;
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

std::uint32_t syncs_deleted = 0u;
void delete_sync(GLsync) { syncs_deleted++; }

GLenum lost_context_error()
{
    errors_asked++;
    return 0x0507u;
}

bool fixed_function_for(GLint mask, std::uint32_t errors)
{
    OpenGlBackend backend;
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
{ OpenGlBackend backend;
  CHECK(backend.knows(av::glproto::kCmd_glClear));
  CHECK(!backend.knows(0x3FFFFu));
  CHECK(!backend.knows(0x2FFFFu));
  CHECK(!backend.knows(0u)); }

TEST(opengl_backend, a_fence_is_found_only_by_the_token_it_was_bound_to)
{ OpenGlBackend backend;
  CHECK(backend.fence_for(7u) == nullptr);
  CHECK(backend.bind_fence(7u, &fence));
  CHECK(backend.fence_for(7u) == &fence);
  CHECK(backend.fence_for(8u) == nullptr);
  CHECK(backend.forget_fence(7u) == &fence);
  CHECK(backend.fence_for(7u) == nullptr);
  CHECK(backend.forget_fence(7u) == nullptr); }

TEST(opengl_backend, guest_bytes_answers_inside_the_region_and_nowhere_else)
{ OpenGlBackend backend;
  CHECK(backend.guest_bytes(0u, 1u) == nullptr);
  av::ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, memory_over(region, sizeof region)), av::Status::ok);
  CHECK(backend.adopt(blob));
  CHECK(backend.guest_bytes(0u, sizeof region) == region);
  CHECK(backend.guest_bytes(16u, 32u) == region + 16u);
  CHECK(backend.guest_bytes(0u, sizeof region + 1u) == nullptr);
  CHECK(backend.guest_bytes(~std::uint64_t{0}, 1u) == nullptr); }

TEST(opengl_backend, a_gpu_with_no_display_opens_no_context)
{ OpenGlBackend backend;
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
  OpenGlBackend unloaded;
  CHECK(OpenGlBackendProbe::granted_fixed_function(unloaded));
  OpenGlBackendProbe::gl(unloaded).glGetIntegerv = profile_integers;
  CHECK(OpenGlBackendProbe::granted_fixed_function(unloaded)); }

TEST(opengl_backend, without_a_context_nothing_runs)
{ OpenGlBackend backend;
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

TEST(opengl_backend, adopts_one_import_as_its_region)
{ OpenGlBackend backend;
  av::ImportedBlob first;
  CHECK_STATUS(first.take(1u, memory_over(region, sizeof region)), av::Status::ok);
  CHECK(backend.adopt(first));
  CHECK(first.wire_handle() != 0u);
  CHECK(first.object() != 0u);
  CHECK(backend.guest_bytes(4096u, 4096u) == region + 4096u);
  CHECK(backend.guest_bytes(4096u, 4097u) == nullptr);
  av::ImportedBlob second;
  CHECK_STATUS(second.take(2u, memory_over(second_region, sizeof second_region)), av::Status::ok);
  CHECK(!backend.adopt(second));
  CHECK_EQ(second.wire_handle(), 0u);
  backend.abandon(second);
  CHECK(backend.guest_bytes(0u, 1u) == region);
  backend.abandon(first);
  CHECK_EQ(first.wire_handle(), 0u);
  CHECK_EQ(first.object(), 0u);
  CHECK(backend.guest_bytes(0u, 1u) == nullptr);
  CHECK(backend.adopt(second));
  CHECK(backend.guest_bytes(0u, 4096u) == second_region);
  backend.abandon(second);
  CHECK(backend.guest_bytes(0u, 1u) == nullptr);
  backend.shut_down();
  av::SourceMemory unmapped(1u, 3u, 0u, AV_MEMORY_WRITE_BACK, av::ExportedMemory(3u, -1, nullptr, 4096u));
  av::ImportedBlob shared;
  CHECK_STATUS(shared.take(3u, unmapped), av::Status::ok);
  CHECK(!backend.adopt(shared));
  CHECK_EQ(shared.wire_handle(), 0u);
  CHECK(backend.guest_bytes(0u, 1u) == nullptr); }

TEST(opengl_backend, a_context_on_a_real_gpu_runs_a_command_and_refuses_a_broken_one)
{ av::LoaderHostApis host;
  host.open();
  const av::GlDisplay *display = nullptr;
  for (std::uint32_t i = 0u; i < host.gpu_count() && display == nullptr; i++) {
      display = host.gl_display(i);
      CHECK_EQ(display != nullptr, (host.sub_apis(i) & AV_EXECUTOR_SUB_API_OPENGL) != 0u);
  }
  CHECK(host.gl_display(host.gpu_count()) == nullptr);
  if (display == nullptr) return;

  OpenGlBackend backend;
  CHECK(backend.initialize(display, nullptr, nullptr));
  CHECK(backend.resolved() > 0u);
  OpenGlBackend other;
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
{ OpenGlBackend backend;
  std::uint32_t handle_kind = 9u;
  int           fd = 9;
  CHECK(!backend.export_object(AV_SHARED_MEMORY, 1u, handle_kind, fd));
  CHECK_EQ(handle_kind, 0u);
  CHECK_EQ(fd, -1);
  std::uint64_t made = 9u;
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, 0, 4096u, av::SharedImageShape(), made, detail));
  CHECK_EQ(made, 0u);
  backend.release_object(AV_SHARED_MEMORY, 1u, detail);
  backend.shut_down(); }

TEST(opengl_backend, an_import_of_the_wrong_shape_never_reaches_a_driver)
{ OpenGlBackend backend;
  std::uint64_t made = 9u;
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_OPAQUE_FD, 0, 4096u, av::SharedImageShape(), made, detail));
  CHECK_EQ(made, 0u);
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, -1, 4096u, av::SharedImageShape(), made, detail));
  CHECK(!backend.import_object(AV_SHARED_NATIVE_FENCE + 1u, AV_SHARED_HANDLE_DMA_BUF, 0, 4096u, av::SharedImageShape(), made, detail));
  CHECK(!backend.import_object(0u, AV_SHARED_HANDLE_DMA_BUF, 0, 4096u, av::SharedImageShape(), made, detail));
  CHECK(!backend.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, 0, 0u, av::SharedImageShape(), made, detail));
  CHECK_EQ(made, 0u); }

namespace {

void never_called(void *) {}

}

TEST(opengl_backend, a_signal_changes_nothing_it_waits_on)
{ OpenGlBackend backend;
  backend.bind_signal(&never_called, nullptr);
  CHECK(!backend.poll());
  std::uint64_t marked = 7u, reached = 7u;
  backend.gpu_progress(marked, reached);
  CHECK_EQ(marked, 0u);
  CHECK_EQ(reached, 0u); }

namespace {

std::uint32_t deleted_syncs = 0u;

void count_deleted_sync(GLsync) { deleted_syncs++; }

}

TEST(opengl_backend, a_sync_no_token_can_keep_is_deleted_at_once)
{ OpenGlBackend backend;
  OpenGlBackendProbe::gl(backend).glDeleteSync = count_deleted_sync;
  deleted_syncs = 0u;
  int first = 0, second = 0;
  CHECK(backend.bind_fence(7u, &first));
  CHECK(!backend.bind_fence(7u, &second));
  CHECK_EQ(deleted_syncs, 1u);
  CHECK(!backend.bind_fence(6u, &second));
  CHECK_EQ(deleted_syncs, 2u);
  CHECK(backend.fence_for(7u) == &first);
  CHECK(backend.fence_for(6u) == nullptr);
  CHECK(!backend.bind_fence(8u, nullptr));
  CHECK_EQ(deleted_syncs, 2u);
  CHECK(backend.forget_fence(7u) == &first); }

namespace {

constexpr GLenum gl_already_signaled = 0x911Au, gl_timeout_expired = 0x911Bu, gl_condition_satisfied = 0x911Cu,
                 gl_wait_failed = 0x911Du;
constexpr GLbitfield gl_sync_flush_commands_bit = 0x1u;

GLenum        wait_answer = gl_already_signaled;
GLsync        waited_on = nullptr;
GLbitfield    waited_flags = 0u;
GLuint64      waited_ns = 0u;
std::uint32_t waits = 0u;

GLenum client_wait_sync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    waited_on = sync;
    waited_flags = flags;
    waited_ns = timeout;
    waits++;
    return wait_answer;
}

constexpr std::uint32_t most_deleted = 16u;
GLsync        deleted[most_deleted] = {};
GLsync        last_deleted = nullptr;
std::uint32_t deletes = 0u;

void record_deleted_sync(GLsync sync)
{
    if (deletes < most_deleted) deleted[deletes] = sync;
    last_deleted = sync;
    deletes++;
}

char syncs[av::SyncTokens::max_live + 8u];

GLsync sync_at(std::uint32_t index) { return reinterpret_cast<GLsync>(&syncs[index]); }

void fill_waiting(OpenGlBackend &backend, GLenum answer)
{
    av::glproto::RealGl &gl = OpenGlBackendProbe::gl(backend);
    gl.glClientWaitSync = client_wait_sync;
    gl.glDeleteSync = record_deleted_sync;
    wait_answer = answer;
    waited_on = nullptr;
    waits = 0u;
    deletes = 0u;
    for (std::uint32_t i = 0u; i < av::SyncTokens::max_live; i++) CHECK(backend.bind_fence(i + 1u, sync_at(i)));
    CHECK_EQ(waits, 0u);
}

bool delete_through_decoder(OpenGlBackend &backend, std::uint64_t token)
{
    std::uint8_t frame[16];
    WireWriter   writer(frame, sizeof frame);
    writer.write_pod<std::uint64_t>(token);
    std::uint8_t   block[256];
    WireArena      arena(block, sizeof block);
    std::uint8_t   reply[16];
    WireReader     payload(frame, writer.offset());
    WireWriter     answer(reply, sizeof reply);
    return av::glproto::dispatch_command(OpenGlBackendProbe::gl(backend), av::glproto::kCmd_glDeleteSync, payload,
                                         answer, arena);
}

}

TEST(opengl_backend, a_full_table_retires_its_oldest_once_the_gpu_passed_it)
{ OpenGlBackend backend;
  fill_waiting(backend, gl_already_signaled);
  const std::uint64_t next = av::SyncTokens::max_live + 1u;
  CHECK(backend.bind_fence(next, sync_at(next - 1u)));
  CHECK_EQ(waits, 1u);
  CHECK(waited_on == sync_at(0u));
  CHECK_EQ(waited_flags, gl_sync_flush_commands_bit);
  CHECK(waited_ns > 0u);
  CHECK_EQ(deletes, 0u);
  CHECK(backend.fence_for(1u) == sync_at(0u));
  CHECK(backend.fence_for(next) == sync_at(next - 1u));
  wait_answer = gl_condition_satisfied;
  CHECK(backend.bind_fence(next + 1u, sync_at(next)));
  CHECK_EQ(waits, 2u);
  CHECK(waited_on == sync_at(1u));
  CHECK_EQ(deletes, 1u);
  CHECK(deleted[0] == sync_at(1u));
  CHECK(backend.fence_for(2u) == sync_at(0u));
  CHECK(backend.fence_for(1u) == sync_at(0u));
  CHECK_EQ(OpenGlBackendProbe::live_syncs(backend), av::SyncTokens::max_live);
  backend.shut_down();
  CHECK_EQ(deletes, 1u + av::SyncTokens::max_live + 1u);
  CHECK(last_deleted == sync_at(0u));
  CHECK(backend.fence_for(1u) == nullptr); }

TEST(opengl_backend, a_full_table_whose_oldest_never_signals_refuses_the_newest)
{ const GLenum unsignaled[] = {gl_timeout_expired, gl_wait_failed};
  for (const GLenum answer : unsignaled) {
      OpenGlBackend backend;
      fill_waiting(backend, answer);
      const std::uint64_t next = av::SyncTokens::max_live + 1u;
      CHECK(!backend.bind_fence(next, sync_at(next - 1u)));
      CHECK_EQ(waits, 1u);
      CHECK_EQ(deletes, 1u);
      CHECK(deleted[0] == sync_at(next - 1u));
      CHECK(backend.fence_for(next) == nullptr);
      CHECK(backend.fence_for(1u) == sync_at(0u));
      wait_answer = gl_already_signaled;
      CHECK(backend.bind_fence(next + 1u, sync_at(next)));
      CHECK(backend.fence_for(1u) == sync_at(0u));
      backend.shut_down();
  } }

TEST(opengl_backend, a_full_table_with_no_way_to_wait_refuses_the_newest)
{ OpenGlBackend backend;
  fill_waiting(backend, gl_already_signaled);
  OpenGlBackendProbe::gl(backend).glClientWaitSync = nullptr;
  const std::uint64_t next = av::SyncTokens::max_live + 1u;
  CHECK(!backend.bind_fence(next, sync_at(next - 1u)));
  CHECK_EQ(deletes, 1u);
  CHECK(backend.fence_for(1u) == sync_at(0u));
  backend.shut_down(); }

TEST(opengl_backend, deleting_a_retired_sync_through_the_decoder_spares_the_stand_in)
{ OpenGlBackend backend;
  fill_waiting(backend, gl_already_signaled);
  OpenGlBackendProbe::install_hooks(backend);
  const std::uint64_t next = av::SyncTokens::max_live + 1u;
  CHECK(backend.bind_fence(next, sync_at(next - 1u)));
  CHECK(backend.bind_fence(next + 1u, sync_at(next)));
  CHECK_EQ(deletes, 1u);
  CHECK(delete_through_decoder(backend, 1u));
  CHECK_EQ(deletes, 2u);
  CHECK(deleted[1] == nullptr);
  CHECK(backend.fence_for(1u) == nullptr);
  CHECK(backend.fence_for(2u) == sync_at(0u));
  CHECK(delete_through_decoder(backend, next));
  CHECK(deleted[2] == sync_at(next - 1u));
  CHECK(backend.fence_for(next) == nullptr);
  CHECK(delete_through_decoder(backend, next + 100u));
  CHECK(deleted[3] == nullptr);
  OpenGlBackendProbe::gl(backend).glDeleteSync = nullptr;
  CHECK(!delete_through_decoder(backend, 2u));
  CHECK(backend.fence_for(2u) == sync_at(0u));
  OpenGlBackendProbe::gl(backend).glDeleteSync = record_deleted_sync;
  backend.shut_down(); }

TEST(opengl_backend, a_lost_context_is_a_lost_device_and_an_error_is_not)
{ { OpenGlBackend backend;
    av::glproto::RealGl &gl = OpenGlBackendProbe::gl(backend);
    gl.glGetError = pending_error;
    errors_left = 3u;
    OpenGlBackendProbe::made_current(backend);
    backend.end_batch();
    CHECK(!backend.lost()); }
  { OpenGlBackend backend;
    av::glproto::RealGl &gl = OpenGlBackendProbe::gl(backend);
    gl.glGetError = lost_context_error;
    errors_asked = 0u;
    OpenGlBackendProbe::made_current(backend);
    backend.end_batch();
    CHECK(backend.lost());
    CHECK_EQ(errors_asked, 8u); } }

TEST(opengl_backend, shutting_down_deletes_every_sync_it_holds)
{ OpenGlBackend backend;
  av::glproto::RealGl &gl = OpenGlBackendProbe::gl(backend);
  gl.glDeleteSync = delete_sync;
  syncs_deleted = 0u;
  int first_sync = 0, second_sync = 0;
  CHECK(backend.bind_fence(3u, &first_sync));
  CHECK(backend.bind_fence(4u, &second_sync));
  backend.shut_down();
  CHECK_EQ(syncs_deleted, 2u);
  CHECK(backend.fence_for(3u) == nullptr);
  CHECK(backend.fence_for(4u) == nullptr);
  OpenGlBackend unresolved;
  CHECK(unresolved.bind_fence(5u, &first_sync));
  unresolved.shut_down();
  CHECK(unresolved.fence_for(5u) == nullptr);
  CHECK_EQ(syncs_deleted, 2u); }
