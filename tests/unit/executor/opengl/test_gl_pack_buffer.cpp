#include "executor/opengl/gl_pack_buffer.hpp"
#include "executor/owned_fd.hpp"
#include "executor/shim/gl_shared_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/opengl/gl_real_dispatch.h"
#include "proto/wire/guest_bytes.hpp"

#include <cstdint>
#include <fcntl.h>
#include <unistd.h>

using av::GlPackBuffer;
using av::GlSharedBuffer;
using av::GuestBytes;
using av::OwnedFd;

namespace {

constexpr GLenum pixel_pack_buffer = 0x88EBu;
constexpr GLenum gpu_commands_complete = 0x9117u;
constexpr std::uint64_t slot = GuestBytes::slot_bytes;

std::uint32_t binds = 0u;
GLenum        bound_target = 0u;
GLuint        bound_name = 7u;
std::uint32_t fences = 0u;
GLenum        fenced_condition = 0u;
std::uint32_t waits = 0u;
GLbitfield    waited_flags = 0u;
GLuint64      waited_for = 0u;
std::uint32_t deletes = 0u;
bool          refuse_fence = false;

void bind_buffer(GLenum target, GLuint name)
{
    binds++;
    bound_target = target;
    bound_name = name;
}

GLsync fence_sync(GLenum condition, GLbitfield)
{
    fences++;
    fenced_condition = condition;
    return refuse_fence ? nullptr : reinterpret_cast<GLsync>(std::uintptr_t{0x1010u});
}

GLenum client_wait(GLsync, GLbitfield flags, GLuint64 timeout)
{
    waits++;
    waited_flags = flags;
    waited_for = timeout;
    return 0x911Au;
}

void delete_sync(GLsync) { deletes++; }

struct Rig {
    av::glproto::RealGl gl;
    GlSharedBuffer      buffers;
    GlPackBuffer        pack{gl, buffers};

    Rig()
    {
        binds = 0u;
        bound_target = 0u;
        bound_name = 7u;
        fences = 0u;
        waits = 0u;
        deletes = 0u;
        refuse_fence = false;
        gl.glBindBuffer = bind_buffer;
        gl.glFenceSync = fence_sync;
        gl.glClientWaitSync = client_wait;
        gl.glDeleteSync = delete_sync;
    }
};

int pipe_end(int (&ends)[2])
{
    CHECK_EQ(::pipe(ends), 0);
    return ends[0];
}

bool open_descriptor(int fd) { return ::fcntl(fd, F_GETFD) != -1; }

}

TEST(gl_pack_buffer, a_new_pack_buffer_packs_nothing)
{ Rig rig;
  CHECK(!rig.pack.packing());
  void *offset = reinterpret_cast<void *>(0x1u);
  CHECK(!rig.pack.bind(0u, 16u, offset));
  CHECK(offset == nullptr);
  CHECK_EQ(binds, 0u); }

TEST(gl_pack_buffer, adopting_a_descriptor_takes_it_and_abandoning_the_segment_closes_it)
{ Rig rig;
  int  ends[2] = {};
  OwnedFd descriptor(pipe_end(ends));
  ::close(ends[1]);
  rig.pack.adopt(descriptor, 0u, slot);
  CHECK(!descriptor.held());
  CHECK(open_descriptor(ends[0]));
  rig.pack.abandon(0u);
  CHECK(!open_descriptor(ends[0])); }

TEST(gl_pack_buffer, adopting_again_over_a_segment_closes_the_descriptor_it_held)
{ Rig rig;
  int first_ends[2] = {};
  int second_ends[2] = {};
  OwnedFd first(pipe_end(first_ends));
  OwnedFd second(pipe_end(second_ends));
  ::close(first_ends[1]);
  ::close(second_ends[1]);
  rig.pack.adopt(first, slot, slot);
  CHECK(open_descriptor(first_ends[0]));
  rig.pack.adopt(second, slot, slot);
  CHECK(!open_descriptor(first_ends[0]));
  CHECK(open_descriptor(second_ends[0]));
  rig.pack.abandon_all();
  CHECK(!open_descriptor(second_ends[0])); }

TEST(gl_pack_buffer, abandoning_what_was_never_adopted_or_lies_past_the_slots_changes_nothing)
{ Rig rig;
  int  ends[2] = {};
  OwnedFd descriptor(pipe_end(ends));
  ::close(ends[1]);
  rig.pack.adopt(descriptor, 2u * slot, slot);
  rig.pack.abandon(slot);
  rig.pack.abandon(slot * GuestBytes::slot_count);
  rig.pack.abandon(~std::uint64_t{0});
  CHECK(open_descriptor(ends[0]));
  rig.pack.abandon_all();
  CHECK(!open_descriptor(ends[0])); }

TEST(gl_pack_buffer, a_bind_outside_every_segment_or_of_no_bytes_or_past_the_segment_binds_nothing)
{ Rig rig;
  int  ends[2] = {};
  OwnedFd descriptor(pipe_end(ends));
  ::close(ends[1]);
  rig.pack.adopt(descriptor, slot, slot / 2u);
  void *offset = reinterpret_cast<void *>(0x1u);
  CHECK(!rig.pack.bind(0u, 16u, offset));
  CHECK(offset == nullptr);
  CHECK(!rig.pack.bind(slot, 0u, offset));
  CHECK(!rig.pack.bind(slot + slot / 2u - 8u, 16u, offset));
  CHECK(!rig.pack.bind(3u * slot, 16u, offset));
  CHECK(!rig.pack.bind(slot * GuestBytes::slot_count, 16u, offset));
  CHECK_EQ(binds, 0u);
  CHECK(!rig.pack.packing());
  rig.pack.release(); }

TEST(gl_pack_buffer, a_segment_the_host_cannot_make_a_buffer_for_is_refused_once_and_never_bound)
{ Rig rig;
  int  ends[2] = {};
  OwnedFd descriptor(pipe_end(ends));
  ::close(ends[1]);
  rig.pack.adopt(descriptor, slot, slot);
  void *offset = reinterpret_cast<void *>(0x1u);
  CHECK(!rig.pack.bind(slot + 64u, 128u, offset));
  CHECK(offset == nullptr);
  CHECK(!rig.pack.bind(slot + 64u, 128u, offset));
  CHECK_EQ(binds, 0u);
  CHECK(!rig.pack.packing());
  rig.pack.sweep();
  rig.pack.release();
  CHECK(open_descriptor(ends[0]));
  rig.pack.abandon_all();
  CHECK(!open_descriptor(ends[0])); }

TEST(gl_pack_buffer, a_driver_without_the_calls_it_needs_is_refused_before_any_buffer_is_asked_for)
{ Rig rig;
  rig.gl.glClientWaitSync = nullptr;
  int  ends[2] = {};
  OwnedFd descriptor(pipe_end(ends));
  ::close(ends[1]);
  rig.pack.adopt(descriptor, 0u, slot);
  void *offset = nullptr;
  CHECK(!rig.pack.bind(0u, 16u, offset));
  CHECK_EQ(binds, 0u);
  rig.pack.abandon_all(); }

TEST(gl_pack_buffer, unbinding_unbinds_the_target_and_waits_for_the_driver_to_finish_writing)
{ Rig rig;
  rig.pack.unbind();
  CHECK_EQ(binds, 1u);
  CHECK(bound_target == pixel_pack_buffer);
  CHECK_EQ(bound_name, 0u);
  CHECK_EQ(fences, 1u);
  CHECK(fenced_condition == gpu_commands_complete);
  CHECK_EQ(waits, 1u);
  CHECK_EQ(waited_flags, 1u);
  CHECK_EQ(waited_for, 1000000000u);
  CHECK_EQ(deletes, 1u); }

TEST(gl_pack_buffer, a_fence_the_driver_refuses_leaves_nothing_to_wait_for_or_delete)
{ Rig rig;
  refuse_fence = true;
  rig.pack.unbind();
  CHECK_EQ(binds, 1u);
  CHECK_EQ(fences, 1u);
  CHECK_EQ(waits, 0u);
  CHECK_EQ(deletes, 0u); }
