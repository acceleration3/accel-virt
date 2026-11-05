#include "executor/eventfd_wake.hpp"
#include "executor/opengl/gl_batch_progress.hpp"
#include "executor/shim/gl_native_fences.hpp"
#include "executor/slice_progress.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/opengl/gl_real_dispatch.h"

#include <cstdint>

using av::EventfdWake;
using av::GlBatchProgress;
using av::GlNativeFences;
using av::SliceProgress;

namespace {

constexpr GLenum already_signalled = 0x911Au;
constexpr GLenum timeout_expired = 0x911Bu;

std::uintptr_t next_sync = 0x1000u;
std::uint32_t  signalled_through = 0u;
std::uint32_t  deleted = 0u;
std::uint32_t  errors_read = 0u;
bool           refuse_sync = false;

GLsync make_sync(GLenum, GLbitfield)
{
    if (refuse_sync) return nullptr;
    next_sync += 0x10u;
    return reinterpret_cast<GLsync>(next_sync);
}

GLenum wait_sync(GLsync sync, GLbitfield, GLuint64)
{
    return reinterpret_cast<std::uintptr_t>(sync) <= 0x1000u + 0x10u * signalled_through ? already_signalled
                                                                                       : timeout_expired;
}

void delete_sync(GLsync) { deleted++; }

GLenum read_error()
{
    errors_read++;
    return 0u;
}

struct Rig {
    av::glproto::RealGl gl;
    GlNativeFences      natives;
    EventfdWake         wake;

    Rig()
    {
        next_sync = 0x1000u;
        signalled_through = 0u;
        deleted = 0u;
        errors_read = 0u;
        refuse_sync = false;
        gl.glFenceSync = make_sync;
        gl.glClientWaitSync = wait_sync;
        gl.glDeleteSync = delete_sync;
        gl.glGetError = read_error;
    }
};

}

TEST(gl_batch_progress, fences_only_where_the_driver_can_make_one)
{ Rig rig;
  GlBatchProgress with(rig.gl, rig.natives, rig.wake);
  CHECK(with.fences());
  av::glproto::RealGl bare;
  GlBatchProgress without(bare, rig.natives, rig.wake);
  CHECK(!without.fences());
  without.fence();
  CHECK_EQ(without.marked(), 0u);
  CHECK(!without.pending());
  CHECK(without.invariants_hold()); }

TEST(gl_batch_progress, marks_each_batch_and_reaches_the_mark_once_its_fence_signals)
{ Rig rig;
  GlBatchProgress progress(rig.gl, rig.natives, rig.wake);
  SliceProgress &seen = progress;
  CHECK_EQ(seen.next(), 1u);
  progress.fence();
  progress.fence();
  CHECK_EQ(progress.marked(), 2u);
  CHECK_EQ(seen.next(), 3u);
  CHECK_EQ(seen.reached(), 0u);
  CHECK(progress.pending());
  CHECK(progress.polled());
  CHECK(progress.invariants_hold());
  CHECK(progress.settle());
  CHECK_EQ(seen.reached(), 0u);
  signalled_through = 1u;
  CHECK(progress.settle());
  CHECK_EQ(seen.reached(), 1u);
  CHECK_EQ(deleted, 1u);
  signalled_through = 2u;
  CHECK(!progress.settle());
  CHECK_EQ(seen.reached(), 2u);
  CHECK(!progress.pending());
  CHECK(!progress.polled());
  CHECK_EQ(deleted, 2u);
  CHECK(progress.invariants_hold()); }

TEST(gl_batch_progress, a_fence_the_driver_refuses_marks_nothing_and_clears_its_error)
{ Rig rig;
  GlBatchProgress progress(rig.gl, rig.natives, rig.wake);
  refuse_sync = true;
  progress.fence();
  CHECK_EQ(progress.marked(), 0u);
  CHECK_EQ(errors_read, 1u);
  CHECK(!progress.pending());
  CHECK(progress.invariants_hold()); }

TEST(gl_batch_progress, the_newest_fence_gives_way_when_too_many_wait)
{ Rig rig;
  GlBatchProgress progress(rig.gl, rig.natives, rig.wake);
  for (std::uint32_t batch = 0u; batch < GlBatchProgress::most_pending + 3u; batch++) progress.fence();
  CHECK_EQ(progress.marked(), GlBatchProgress::most_pending + 3u);
  CHECK_EQ(deleted, 3u);
  CHECK(progress.invariants_hold());
  signalled_through = 1000u;
  CHECK(!progress.settle());
  CHECK_EQ(progress.reached(), GlBatchProgress::most_pending + 3u);
  CHECK(progress.invariants_hold()); }

TEST(gl_batch_progress, release_and_forget_call_everything_reached_and_differ_in_deleting_the_syncs)
{ Rig rig;
  GlBatchProgress released(rig.gl, rig.natives, rig.wake);
  released.fence();
  released.fence();
  released.release();
  CHECK_EQ(deleted, 2u);
  CHECK_EQ(released.reached(), 2u);
  CHECK(!released.pending());
  CHECK(released.invariants_hold());
  GlBatchProgress forgotten(rig.gl, rig.natives, rig.wake);
  forgotten.fence();
  forgotten.forget();
  CHECK_EQ(deleted, 2u);
  CHECK_EQ(forgotten.reached(), forgotten.marked());
  CHECK(forgotten.invariants_hold()); }
