#include "core/log/driver_log.hpp"
#include "executor/eventfd_wake.hpp"
#include "executor/opengl/gl_batch_progress.hpp"
#include "executor/opengl/gl_error_queue.hpp"
#include "executor/opengl/gl_memory_objects.hpp"
#include "executor/opengl/gl_texture_slices.hpp"
#include "executor/shim/gl_dma_buf_images.hpp"
#include "executor/shim/gl_host_import.hpp"
#include "executor/shim/gl_native_fences.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/generated/opengl/gl_real_dispatch.h"
#include "proto/submission/gl_error_word.hpp"

#include <cstdint>

using av::GlBatchProgress;
using av::GlDmaBufImages;
using av::GlErrorQueue;
using av::GlErrorWord;
using av::GlHostImport;
using av::GlMemoryObjects;
using av::GlTextureSlices;

namespace {

constexpr GLenum texture_2d = 0x0DE1u;
constexpr GLenum texture_binding_2d = 0x8069u;
constexpr GLenum texture_cube_map = 0x8513u;
constexpr GLenum texture_3d = 0x806Fu;
constexpr GLenum rgba8 = 0x8058u;
constexpr GLenum dxt5 = 0x83F3u;
constexpr std::uint32_t invalid_operation = 0x0502u;
constexpr GLenum already_signalled = 0x911Au;
constexpr GLenum timeout_expired = 0x911Bu;

std::uintptr_t next_sync = 0x1000u;
std::uint32_t  signalled_through = 0u;
std::uint32_t  driver_error = 0u;
std::uint32_t  error_on_store = 0u;
std::uint32_t  stored = 0u;
GLuint         stored_memory = 0u;
GLuint64       stored_offset = 0u;
GLint          bound_texture = 0;
GLenum         asked_binding = 0u;
std::uint32_t  viewed = 0u;
GLuint         viewed_original = 0u;

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

void note_store(GLuint memory, GLuint64 offset)
{
    stored++;
    stored_memory = memory;
    stored_offset = offset;
    driver_error = error_on_store;
}

void tex_storage_2d(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLuint memory, GLuint64 offset)
{
    note_store(memory, offset);
}

void tex_storage_2d_ms(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean, GLuint memory, GLuint64 offset)
{
    note_store(memory, offset);
}

void tex_storage_3d(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei, GLuint memory, GLuint64 offset)
{
    note_store(memory, offset);
}

void tex_storage_1d(GLenum, GLsizei, GLenum, GLsizei, GLuint memory, GLuint64 offset) { note_store(memory, offset); }

void tex_storage_3d_ms(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei, GLboolean, GLuint memory, GLuint64 offset)
{
    note_store(memory, offset);
}

void texture_storage_2d(GLuint, GLsizei, GLenum, GLsizei, GLsizei, GLuint memory, GLuint64 offset)
{
    note_store(memory, offset);
}

void integer(GLenum pname, GLint *value)
{
    asked_binding = pname;
    *value = bound_texture;
}

void texture_view(GLuint, GLenum, GLuint original, GLenum, GLuint, GLuint, GLuint, GLuint)
{
    viewed++;
    viewed_original = original;
    driver_error = error_on_store;
}

struct Rig {
    av::MockPlatform       platform;
    av::glproto::RealGl    gl;
    GlErrorWord            word;
    GlErrorQueue           errors{gl, word};
    GlHostImport           import;
    av::GlNativeFences     natives;
    av::EventfdWake        wake;
    GlBatchProgress        progress{gl, natives, wake};
    GlMemoryObjects        memories{platform, progress};
    GlDmaBufImages         images{platform};
    GlTextureSlices        slices{platform, gl, memories, images, errors};

    Rig()
    {
        next_sync = 0x1000u;
        signalled_through = 0u;
        driver_error = 0u;
        error_on_store = 0u;
        stored = 0u;
        bound_texture = 0;
        asked_binding = 0u;
        viewed = 0u;
        gl.glFenceSync = make_sync;
        gl.glClientWaitSync = wait_sync;
        gl.glDeleteSync = delete_sync;
        gl.glGetError = read_error;
        gl.glGetIntegerv = integer;
        gl.glTexStorageMem1DEXT = tex_storage_1d;
        gl.glTexStorageMem2DEXT = tex_storage_2d;
        gl.glTexStorageMem2DMultisampleEXT = tex_storage_2d_ms;
        gl.glTexStorageMem3DEXT = tex_storage_3d;
        gl.glTexStorageMem3DMultisampleEXT = tex_storage_3d_ms;
        gl.glTextureStorageMem2DEXT = texture_storage_2d;
        gl.glTextureStorageMem1DEXT = tex_storage_1d;
        gl.glTextureStorageMem2DMultisampleEXT = tex_storage_2d_ms;
        gl.glTextureStorageMem3DEXT = tex_storage_3d;
        gl.glTextureStorageMem3DMultisampleEXT = tex_storage_3d_ms;
        gl.glTextureView = texture_view;
    }
};

}

TEST(gl_texture_slices, installing_hooks_every_texture_storage_entry_and_the_deletion_ahead_of_the_driver)
{ Rig rig;
  rig.slices.install();
  CHECK(rig.gl.av_texture_user == &rig.slices);
  CHECK(rig.gl.av_hook_glTexStorageMem1DEXT != nullptr);
  CHECK(rig.gl.av_hook_glTexStorageMem2DEXT != nullptr);
  CHECK(rig.gl.av_hook_glTexStorageMem2DMultisampleEXT != nullptr);
  CHECK(rig.gl.av_hook_glTexStorageMem3DEXT != nullptr);
  CHECK(rig.gl.av_hook_glTexStorageMem3DMultisampleEXT != nullptr);
  CHECK(rig.gl.av_hook_glTextureStorageMem1DEXT != nullptr);
  CHECK(rig.gl.av_hook_glTextureStorageMem2DEXT != nullptr);
  CHECK(rig.gl.av_hook_glTextureStorageMem2DMultisampleEXT != nullptr);
  CHECK(rig.gl.av_hook_glTextureStorageMem3DEXT != nullptr);
  CHECK(rig.gl.av_hook_glTextureStorageMem3DMultisampleEXT != nullptr);
  CHECK(rig.gl.av_hook_glDeleteTextures != nullptr);
  CHECK(rig.gl.av_hook_glTextureView != nullptr); }

TEST(gl_texture_slices, a_texture_stored_in_a_memory_is_placed_and_the_driver_is_told_the_same)
{ Rig rig;
  CHECK_EQ(rig.memories.add(7u, 0x4000u), av::Status::ok);
  bound_texture = 5;
  CHECK(!rig.slices.on_tex_storage_mem_2d(texture_2d, 1, rgba8, 16, 16, 7u, 0x1000u));
  CHECK_EQ(asked_binding, texture_binding_2d);
  CHECK_EQ(stored, 1u);
  CHECK_EQ(stored_memory, 7u);
  CHECK_EQ(stored_offset, 0x1000u);
  CHECK_EQ(rig.memories.textured(), 1u);
  CHECK_EQ(rig.word.count(1u), 0u);
  CHECK(rig.memories.invariants_hold()); }

TEST(gl_texture_slices, a_storage_whose_footprint_leaves_the_memory_is_an_invalid_value_and_never_reaches_the_driver)
{ Rig rig;
  CHECK_EQ(rig.memories.add(7u, 0x4000u), av::Status::ok);
  bound_texture = 5;
  CHECK(!rig.slices.on_tex_storage_mem_2d(texture_2d, 1, rgba8, 64, 64, 7u, 0u));
  CHECK_EQ(stored, 1u);
  stored = 0u;
  CHECK(!rig.slices.on_tex_storage_mem_2d(texture_2d, 1, rgba8, 64, 65, 7u, 0u));
  CHECK(!rig.slices.on_tex_storage_mem_2d(texture_2d, 1, rgba8, 64, 64, 7u, 4u));
  CHECK(!rig.slices.on_tex_storage_mem_2d(texture_2d, 1, rgba8, 8, 8, 9u, 0u));
  CHECK(!rig.slices.on_tex_storage_mem_2d(texture_2d, 0, rgba8, 8, 8, 7u, 0u));
  CHECK(!rig.slices.on_tex_storage_mem_2d(texture_2d, 1, rgba8, -8, 8, 7u, 0u));
  CHECK_EQ(stored, 0u);
  CHECK_EQ(rig.word.count(1u), 5u);
  CHECK_EQ(rig.memories.textured(), 1u);
  CHECK(rig.memories.invariants_hold()); }

TEST(gl_texture_slices, every_storage_entry_is_bounded_by_its_own_dimensions_levels_samples_and_target)
{ Rig rig;
  CHECK_EQ(rig.memories.add(7u, 0x1000u), av::Status::ok);
  bound_texture = 5;
  CHECK(!rig.slices.on_tex_storage_mem_1d(0x0DE0u, 1, rgba8, 2048, 7u, 0u));
  CHECK_EQ(stored, 0u);
  CHECK(!rig.slices.on_tex_storage_mem_2d_multisample(0x9100u, 4, rgba8, 32, 16, 1u, 7u, 0u));
  CHECK_EQ(stored, 0u);
  CHECK(!rig.slices.on_tex_storage_mem_3d(texture_3d, 1, rgba8, 16, 16, 8, 7u, 0u));
  CHECK_EQ(stored, 0u);
  CHECK(!rig.slices.on_tex_storage_mem_3d_multisample(0x9102u, 3, rgba8, 8, 8, 8, 1u, 7u, 0u));
  CHECK_EQ(stored, 0u);
  CHECK(!rig.slices.on_tex_storage_mem_2d(texture_cube_map, 1, rgba8, 16, 16, 7u, 0u));
  CHECK_EQ(stored, 0u);
  CHECK(!rig.slices.on_tex_storage_mem_2d(texture_2d, 5, rgba8, 16, 16, 7u, 0u));
  CHECK_EQ(stored, 1u);
  CHECK(!rig.slices.on_tex_storage_mem_2d(texture_2d, 1, dxt5, 64, 64, 7u, 0u));
  CHECK_EQ(stored, 2u);
  CHECK_EQ(rig.word.count(1u), 5u); }

TEST(gl_texture_slices, a_target_that_is_not_storage_or_has_no_texture_bound_is_left_to_the_driver_to_judge)
{ Rig rig;
  CHECK_EQ(rig.memories.add(7u, 0x1000u), av::Status::ok);
  bound_texture = 5;
  CHECK(rig.slices.on_tex_storage_mem_2d(0x8064u, 1, rgba8, 4096, 4096, 7u, 0u));
  bound_texture = 0;
  CHECK(rig.slices.on_tex_storage_mem_2d(texture_2d, 1, rgba8, 4096, 4096, 7u, 0u));
  CHECK_EQ(stored, 0u);
  CHECK_EQ(rig.memories.textured(), 0u); }

TEST(gl_texture_slices, a_named_texture_is_bounded_by_the_target_it_was_made_with)
{ Rig rig;
  CHECK_EQ(rig.memories.add(7u, 0x4000u), av::Status::ok);
  const GLuint flat = 3u, cube = 4u;
  rig.slices.on_textures_created(texture_2d, 1, &flat);
  rig.slices.on_textures_created(texture_cube_map, 1, &cube);
  CHECK(!rig.slices.on_texture_storage_mem_2d(flat, 1, rgba8, 32, 32, 7u, 0u));
  CHECK_EQ(stored, 1u);
  CHECK_EQ(rig.memories.textured(), 1u);
  CHECK(!rig.slices.on_texture_storage_mem_2d(cube, 1, rgba8, 32, 32, 7u, 0u));
  CHECK_EQ(stored, 1u);
  CHECK_EQ(rig.word.count(1u), 1u); }

TEST(gl_texture_slices, a_named_texture_whose_target_is_unknown_is_bounded_by_the_least_target_the_entry_admits)
{ Rig rig;
  CHECK_EQ(rig.memories.add(7u, 0x4000u), av::Status::ok);
  CHECK(!rig.slices.on_texture_storage_mem_2d(5u, 1, rgba8, 32, 32, 7u, 0u));
  CHECK_EQ(stored, 1u);
  CHECK(!rig.slices.on_texture_storage_mem_2d(5u, 1, rgba8, 64, 64, 7u, 0u));
  CHECK_EQ(stored, 2u);
  CHECK(!rig.slices.on_texture_storage_mem_2d(5u, 1, rgba8, 64, 65, 7u, 0u));
  CHECK_EQ(stored, 2u);
  CHECK(!rig.slices.on_texture_storage_mem_3d(6u, 1, rgba8, 16, 16, 16, 7u, 0u));
  CHECK_EQ(stored, 3u);
  CHECK(!rig.slices.on_texture_storage_mem_3d(7u, 1, rgba8, 16, 16, 32, 7u, 0u));
  CHECK_EQ(stored, 3u);
  CHECK(!rig.slices.on_texture_storage_mem_3d_multisample(8u, 2, rgba8, 16, 16, 16, 1u, 7u, 0u));
  CHECK_EQ(stored, 3u);
  CHECK_EQ(rig.word.count(1u), 3u); }

TEST(gl_texture_slices, a_texture_bound_through_the_direct_state_access_extension_is_known_by_that_target)
{ Rig rig;
  rig.slices.install();
  CHECK(rig.gl.av_after_glBindMultiTextureEXT != nullptr);
  CHECK(rig.gl.av_after_glTextureImage1DEXT != nullptr);
  CHECK(rig.gl.av_after_glTextureImage2DEXT != nullptr);
  CHECK(rig.gl.av_after_glTextureImage3DEXT != nullptr);
  CHECK(rig.gl.av_after_glGenTextures != nullptr);
  CHECK_EQ(rig.memories.add(7u, 0x4000u), av::Status::ok);
  rig.gl.av_after_glBindMultiTextureEXT(&rig.slices, 0x84C1u, texture_cube_map, 12u);
  CHECK(!rig.slices.on_texture_storage_mem_2d(12u, 1, rgba8, 32, 32, 7u, 0u));
  CHECK_EQ(stored, 0u);
  CHECK_EQ(rig.word.count(1u), 1u);
  rig.gl.av_after_glBindMultiTextureEXT(&rig.slices, 0x84C1u, texture_2d, 13u);
  CHECK(!rig.slices.on_texture_storage_mem_2d(13u, 1, rgba8, 32, 32, 7u, 0u));
  CHECK_EQ(stored, 1u); }

TEST(gl_texture_slices, a_texture_imaged_through_the_direct_state_access_extension_is_known_by_its_target_and_a_cube_face_by_the_cube_map)
{ Rig rig;
  rig.slices.install();
  CHECK_EQ(rig.memories.add(7u, 0x4000u), av::Status::ok);
  rig.gl.av_after_glTextureImage2DEXT(&rig.slices, 14u, 0x8516u, 0, 0x8058, 32, 32, 0, 0x1908u, 0x1401u, nullptr);
  CHECK(!rig.slices.on_texture_storage_mem_2d(14u, 1, rgba8, 32, 32, 7u, 0u));
  CHECK_EQ(stored, 0u);
  rig.gl.av_after_glTextureImage3DEXT(&rig.slices, 15u, texture_3d, 0, 0x8058, 16, 16, 16, 0, 0x1908u, 0x1401u, nullptr);
  CHECK(!rig.slices.on_texture_storage_mem_3d(15u, 1, rgba8, 16, 16, 16, 7u, 0u));
  CHECK_EQ(stored, 1u);
  rig.gl.av_after_glTextureImage1DEXT(&rig.slices, 16u, 0x0DE0u, 0, 0x8058, 64, 0, 0x1908u, 0x1401u, nullptr);
  CHECK(!rig.slices.on_texture_storage_mem_1d(16u, 1, rgba8, 4096, 7u, 0u));
  CHECK_EQ(stored, 2u);
  CHECK_EQ(rig.word.count(1u), 1u); }

TEST(gl_texture_slices, a_generated_name_forgets_the_target_an_earlier_texture_of_that_name_had)
{ Rig rig;
  CHECK_EQ(rig.memories.add(7u, 0x4000u), av::Status::ok);
  const GLuint reused = 3u;
  rig.slices.on_texture_bound(texture_cube_map, reused);
  CHECK(!rig.slices.on_texture_storage_mem_2d(reused, 1, rgba8, 32, 32, 7u, 0u));
  CHECK_EQ(stored, 0u);
  rig.slices.on_textures_generated(1, &reused);
  rig.slices.on_texture_bound(texture_2d, reused);
  CHECK(!rig.slices.on_texture_storage_mem_2d(reused, 1, rgba8, 32, 32, 7u, 0u));
  CHECK_EQ(stored, 1u);
  rig.slices.on_textures_created(texture_cube_map, 1, &reused);
  CHECK(!rig.slices.on_texture_storage_mem_2d(reused, 1, rgba8, 32, 32, 7u, 0u));
  CHECK_EQ(stored, 1u); }

TEST(gl_texture_slices, a_texture_is_known_by_the_target_it_was_created_or_first_bound_with_until_it_is_deleted)
{ Rig rig;
  const GLuint made[] = {3u, 4u, 0u};
  rig.slices.on_textures_created(texture_2d, 3, made);
  rig.slices.on_textures_created(0x8064u, 1, made);
  rig.slices.on_textures_created(texture_cube_map, 1, nullptr);
  rig.slices.on_texture_bound(texture_cube_map, 3u);
  rig.slices.on_texture_bound(texture_3d, 9u);
  rig.slices.on_texture_bound(0x8515u, 10u);
  CHECK(rig.memories.add(7u, 0x4000u) == av::Status::ok);
  CHECK(!rig.slices.on_texture_storage_mem_2d(3u, 1, rgba8, 32, 32, 7u, 0u));
  CHECK_EQ(stored, 1u);
  CHECK(!rig.slices.on_texture_storage_mem_3d(9u, 1, rgba8, 16, 16, 16, 7u, 0u));
  CHECK_EQ(stored, 2u);
  rig.slices.on_texture_bound(texture_2d, 11u);
  CHECK(!rig.slices.on_texture_storage_mem_2d(11u, 1, rgba8, 32, 32, 7u, 0u));
  CHECK_EQ(stored, 3u);
  const GLuint gone[] = {3u};
  CHECK(rig.slices.on_delete_textures(1, gone));
  rig.slices.on_texture_bound(texture_cube_map, 3u);
  CHECK(!rig.slices.on_texture_storage_mem_2d(3u, 1, rgba8, 32, 32, 7u, 0u));
  CHECK_EQ(stored, 3u);
  CHECK_EQ(rig.word.count(1u), 1u); }

TEST(gl_texture_slices, an_error_the_driver_raises_storing_a_texture_is_recorded_and_places_nothing)
{ Rig rig;
  CHECK_EQ(rig.memories.add(7u, 0x4000u), av::Status::ok);
  bound_texture = 5;
  error_on_store = invalid_operation;
  CHECK(!rig.slices.on_tex_storage_mem_2d(texture_2d, 1, rgba8, 16, 16, 7u, 0u));
  CHECK_EQ(rig.word.count(2u), 1u);
  CHECK_EQ(rig.memories.textured(), 0u);
  CHECK(rig.memories.invariants_hold()); }

TEST(gl_texture_slices, deleting_a_texture_leaves_its_memory_and_a_deleted_memory_waits_for_its_last_texture)
{ Rig rig;
  CHECK_EQ(rig.memories.add(7u, 0x4000u), av::Status::ok);
  bound_texture = 5;
  CHECK(!rig.slices.on_tex_storage_mem_2d(texture_2d, 1, rgba8, 16, 16, 7u, 0u));
  bound_texture = 6;
  CHECK(!rig.slices.on_tex_storage_mem_2d(texture_2d, 1, rgba8, 16, 16, 7u, 0x1000u));
  CHECK_EQ(rig.memories.textured(), 2u);
  rig.memories.retire(7u);
  rig.progress.fence();
  signalled_through = 1u;
  CHECK(!rig.progress.settle());
  rig.memories.reclaim(rig.import);
  CHECK(rig.memories.holds(7u));
  const GLuint first[] = {5u, 99u};
  CHECK(rig.slices.on_delete_textures(2, first));
  rig.memories.reclaim(rig.import);
  CHECK(rig.memories.holds(7u));
  const GLuint second[] = {6u};
  CHECK(rig.slices.on_delete_textures(1, second));
  CHECK(rig.slices.on_delete_textures(1, nullptr));
  CHECK(rig.slices.on_delete_textures(-1, second));
  rig.progress.fence();
  signalled_through = 2u;
  CHECK(!rig.progress.settle());
  rig.memories.reclaim(rig.import);
  CHECK(!rig.memories.holds(7u));
  CHECK_EQ(rig.memories.textured(), 0u);
  CHECK(rig.memories.invariants_hold()); }

TEST(gl_texture_slices, a_view_of_a_placed_texture_keeps_the_memory_after_the_original_is_deleted)
{ Rig rig;
  CHECK_EQ(rig.memories.add(7u, 0x4000u), av::Status::ok);
  bound_texture = 5;
  CHECK(!rig.slices.on_tex_storage_mem_2d(texture_2d, 1, rgba8, 16, 16, 7u, 0u));
  CHECK(!rig.slices.on_texture_view(8u, texture_2d, 5u, rgba8, 0u, 1u, 0u, 1u));
  CHECK_EQ(viewed, 1u);
  CHECK_EQ(viewed_original, 5u);
  CHECK_EQ(rig.memories.textured(), 2u);
  const GLuint original[] = {5u};
  CHECK(rig.slices.on_delete_textures(1, original));
  CHECK_EQ(rig.memories.textured(), 1u);
  rig.memories.retire(7u);
  rig.progress.fence();
  signalled_through = 1u;
  CHECK(!rig.progress.settle());
  rig.memories.reclaim(rig.import);
  CHECK(rig.memories.holds(7u));
  CHECK(!rig.slices.on_texture_view(9u, texture_2d, 77u, rgba8, 0u, 1u, 0u, 1u));
  CHECK_EQ(rig.memories.textured(), 1u);
  error_on_store = invalid_operation;
  CHECK(!rig.slices.on_texture_view(10u, texture_2d, 8u, rgba8, 0u, 1u, 0u, 1u));
  CHECK_EQ(rig.memories.textured(), 1u);
  CHECK_EQ(rig.word.count(2u), 1u);
  const GLuint view[] = {8u};
  CHECK(rig.slices.on_delete_textures(1, view));
  rig.progress.fence();
  signalled_through = 2u;
  CHECK(!rig.progress.settle());
  rig.memories.reclaim(rig.import);
  CHECK(!rig.memories.holds(7u));
  CHECK(rig.memories.invariants_hold()); }

