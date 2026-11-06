#include "core/log/driver_log.hpp"
#include "executor/eventfd_wake.hpp"
#include "executor/opengl/gl_batch_progress.hpp"
#include "executor/opengl/gl_buffer_slices.hpp"
#include "executor/opengl/gl_error_queue.hpp"
#include "executor/shim/gl_host_import.hpp"
#include "executor/shim/gl_native_fences.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/generated/glmem/glmem_protocol_ids.h"
#include "proto/generated/opengl/gl_real_dispatch.h"
#include "proto/submission/gl_error_word.hpp"
#include "proto/wire/guest_bytes.hpp"

#include <cstdint>

using av::GlBatchProgress;
using av::GlBufferSlices;
using av::GlErrorQueue;
using av::GlErrorWord;
using av::GlHostImport;

namespace {

constexpr std::uint32_t invalid_operation = 0x0502u;
constexpr GLenum        array_buffer = 0x8892u;
constexpr GLenum        array_buffer_binding = 0x8894u;
constexpr GLenum        already_signalled = 0x911Au;
constexpr GLenum        timeout_expired = 0x911Bu;

std::uintptr_t next_sync = 0x1000u;
std::uint32_t  signalled_through = 0u;
std::uint32_t  driver_error = 0u;
std::uint32_t  error_on_store = 0u;
std::uint32_t  stored = 0u;
GLuint         stored_buffer = 0u;
GLsizeiptr     stored_size = 0;
GLuint         stored_memory = 0u;
GLuint64       stored_offset = 0u;
GLint          bound_buffer = 0;
GLenum         asked_binding = 0u;

GLsync make_sync(GLenum, GLbitfield)
{
    next_sync += 0x10u;
    return reinterpret_cast<GLsync>(next_sync);
}

GLenum wait_sync(GLsync sync, GLbitfield, GLuint64)
{
    return reinterpret_cast<std::uintptr_t>(sync) <= 0x1000u + 0x10u * signalled_through ? already_signalled
                                                                                       : timeout_expired;
}

void delete_sync(GLsync) {}

GLenum read_error()
{
    const std::uint32_t error = driver_error;
    driver_error = 0u;
    return error;
}

void storage_mem(GLuint buffer, GLsizeiptr size, GLuint memory, GLuint64 offset)
{
    stored++;
    stored_buffer = buffer;
    stored_size = size;
    stored_memory = memory;
    stored_offset = offset;
    driver_error = error_on_store;
}

void integer(GLenum pname, GLint *value)
{
    asked_binding = pname;
    *value = bound_buffer;
}

struct Rig {
    av::MockPlatform       platform;
    av::RecordingLog       sink;
    av::DriverLog          lines{sink, "gl: "};
    av::glproto::RealGl    gl;
    GlErrorWord            word;
    GlErrorQueue           errors{gl, word};
    GlHostImport           import;
    av::GlNativeFences     natives;
    av::EventfdWake        wake;
    GlBatchProgress        progress{gl, natives, wake};
    av::GuestBytes         guest;
    GlBufferSlices         slices{platform, gl, import, progress, errors, guest, lines};

    Rig()
    {
        next_sync = 0x1000u;
        signalled_through = 0u;
        driver_error = 0u;
        error_on_store = 0u;
        stored = 0u;
        bound_buffer = 0;
        asked_binding = 0u;
        gl.glFenceSync = make_sync;
        gl.glClientWaitSync = wait_sync;
        gl.glDeleteSync = delete_sync;
        gl.glGetError = read_error;
        gl.glNamedBufferStorageMemEXT = storage_mem;
        gl.glGetIntegerv = integer;
    }
};

}

TEST(gl_buffer_slices, installing_hooks_the_four_calls_it_judges_ahead_of_the_driver)
{ Rig rig;
  CHECK(rig.gl.av_hook_glBufferStorageMemEXT == nullptr);
  rig.slices.install();
  CHECK(rig.gl.av_hook_user == &rig.slices);
  CHECK(rig.gl.av_hook_glBufferStorageMemEXT != nullptr);
  CHECK(rig.gl.av_hook_glNamedBufferStorageMemEXT != nullptr);
  CHECK(rig.gl.av_hook_glDeleteMemoryObjectsEXT != nullptr);
  CHECK(rig.gl.av_hook_glDeleteBuffers != nullptr);
  CHECK(rig.slices.knows(av::glmemproto::kCmd_PlaceBufferSlice));
  CHECK(rig.slices.knows(av::glmemproto::kCmd_ForgetBufferSlice));
  CHECK(!rig.slices.knows(0x10001u));
  CHECK(rig.slices.invariants_hold()); }

TEST(gl_buffer_slices, a_host_that_cannot_import_buffers_is_not_offered_to_place_any)
{ Rig rig;
  std::uint32_t supported = 7u;
  CHECK(rig.slices.on_QueryBufferSlicesGL(supported));
  CHECK_EQ(supported, 0u);
  CHECK(rig.progress.fences());
  CHECK(!rig.import.places_buffers());
  CHECK_EQ(rig.slices.add_memory(7u, 0x4000u), av::Status::ok);
  std::uint32_t placed = 7u;
  CHECK(rig.slices.on_PlaceBufferSlice(3u, 7u, 0u, 0x1000u, placed));
  CHECK_EQ(placed, 0u);
  CHECK_EQ(rig.slices.placed(), 0u);
  CHECK_EQ(stored, 0u); }

TEST(gl_buffer_slices, a_named_buffer_stored_in_a_memory_is_placed_and_the_driver_is_told_the_same)
{ Rig rig;
  CHECK_EQ(rig.slices.add_memory(7u, 0x4000u), av::Status::ok);
  CHECK_EQ(rig.slices.count(), 1u);
  CHECK(!rig.slices.on_named_buffer_storage_mem(3u, 0x1000, 7u, 0x2000u));
  CHECK_EQ(stored, 1u);
  CHECK_EQ(stored_buffer, 3u);
  CHECK_EQ(stored_size, 0x1000);
  CHECK_EQ(stored_memory, 7u);
  CHECK_EQ(stored_offset, 0x2000u);
  CHECK_EQ(rig.slices.placed(), 1u);
  CHECK_EQ(rig.word.count(1u), 0u);
  CHECK(rig.slices.invariants_hold()); }

TEST(gl_buffer_slices, a_store_outside_the_memory_or_of_nothing_or_into_unknown_memory_is_an_invalid_value)
{ Rig rig;
  CHECK_EQ(rig.slices.add_memory(7u, 0x4000u), av::Status::ok);
  CHECK(!rig.slices.on_named_buffer_storage_mem(3u, 0x1000, 7u, 0x4000u));
  CHECK(!rig.slices.on_named_buffer_storage_mem(3u, 0, 7u, 0u));
  CHECK(!rig.slices.on_named_buffer_storage_mem(3u, -4, 7u, 0u));
  CHECK(!rig.slices.on_named_buffer_storage_mem(3u, 0x1000, 9u, 0u));
  CHECK_EQ(rig.word.count(1u), 4u);
  CHECK_EQ(stored, 0u);
  CHECK_EQ(rig.slices.placed(), 0u);
  CHECK(rig.slices.invariants_hold()); }

TEST(gl_buffer_slices, an_error_the_driver_raises_storing_a_buffer_is_recorded_and_places_nothing)
{ Rig rig;
  CHECK_EQ(rig.slices.add_memory(7u, 0x4000u), av::Status::ok);
  error_on_store = invalid_operation;
  CHECK(!rig.slices.on_named_buffer_storage_mem(3u, 0x1000, 7u, 0u));
  CHECK_EQ(rig.word.count(2u), 1u);
  CHECK_EQ(rig.slices.placed(), 0u);
  CHECK(rig.slices.invariants_hold()); }

TEST(gl_buffer_slices, a_store_into_the_buffer_bound_to_a_target_goes_to_that_buffer)
{ Rig rig;
  CHECK_EQ(rig.slices.add_memory(7u, 0x4000u), av::Status::ok);
  bound_buffer = 5;
  CHECK(!rig.slices.on_buffer_storage_mem(array_buffer, 0x800, 7u, 0u));
  CHECK_EQ(asked_binding, array_buffer_binding);
  CHECK_EQ(stored_buffer, 5u);
  CHECK_EQ(rig.slices.placed(), 1u);
  stored = 0u;
  bound_buffer = 0;
  CHECK(rig.slices.on_buffer_storage_mem(array_buffer, 0x800, 7u, 0u));
  CHECK(rig.slices.on_buffer_storage_mem(0x1234u, 0x800, 7u, 0u));
  CHECK_EQ(stored, 0u);
  CHECK_EQ(rig.slices.placed(), 1u); }

TEST(gl_buffer_slices, a_buffer_deleted_or_forgotten_leaves_its_memory_and_the_driver_still_deletes_it)
{ Rig rig;
  CHECK_EQ(rig.slices.add_memory(7u, 0x4000u), av::Status::ok);
  CHECK(!rig.slices.on_named_buffer_storage_mem(3u, 0x1000, 7u, 0u));
  CHECK(!rig.slices.on_named_buffer_storage_mem(4u, 0x1000, 7u, 0x1000u));
  CHECK_EQ(rig.slices.placed(), 2u);
  const GLuint gone[] = {3u, 99u};
  CHECK(rig.slices.on_delete_buffers(2, gone));
  CHECK_EQ(rig.slices.placed(), 1u);
  CHECK(rig.slices.on_delete_buffers(0, gone));
  CHECK(rig.slices.on_delete_buffers(2, nullptr));
  CHECK_EQ(rig.slices.placed(), 1u);
  CHECK(rig.slices.on_ForgetBufferSlice(4u));
  CHECK_EQ(rig.slices.placed(), 0u);
  CHECK(rig.slices.on_ForgetBufferSlice(4u));
  CHECK(rig.slices.invariants_hold()); }

TEST(gl_buffer_slices, a_memory_deleted_is_kept_until_nothing_is_placed_in_it_and_the_gpu_has_passed_its_last_batch)
{ Rig rig;
  CHECK_EQ(rig.slices.add_memory(7u, 0x4000u), av::Status::ok);
  CHECK(!rig.slices.on_named_buffer_storage_mem(3u, 0x1000, 7u, 0u));
  const GLuint memories[] = {7u};
  CHECK(!rig.slices.on_delete_memory_objects(1, memories));
  CHECK_EQ(rig.slices.count(), 1u);
  const GLuint buffers[] = {3u};
  CHECK(rig.slices.on_delete_buffers(1, buffers));
  rig.slices.reclaim();
  CHECK_EQ(rig.slices.count(), 1u);
  rig.progress.fence();
  signalled_through = 1u;
  CHECK(!rig.progress.settle());
  rig.slices.reclaim();
  CHECK_EQ(rig.slices.count(), 0u);
  CHECK(rig.slices.invariants_hold()); }

TEST(gl_buffer_slices, retiring_a_memory_by_name_and_deleting_none_change_what_is_held_only_when_asked)
{ Rig rig;
  CHECK_EQ(rig.slices.add_memory(7u, 0x4000u), av::Status::ok);
  CHECK(!rig.slices.on_delete_memory_objects(1, nullptr));
  CHECK(!rig.slices.on_delete_memory_objects(0, nullptr));
  CHECK_EQ(rig.slices.count(), 1u);
  rig.slices.retire_memory(7u);
  rig.progress.fence();
  signalled_through = 1u;
  CHECK(!rig.progress.settle());
  rig.slices.reclaim();
  CHECK_EQ(rig.slices.count(), 0u);
  CHECK_EQ(rig.slices.add_memory(8u, 0x1000u), av::Status::ok);
  rig.slices.destroy_all();
  CHECK_EQ(rig.slices.count(), 0u);
  CHECK_EQ(rig.slices.placed(), 0u); }
