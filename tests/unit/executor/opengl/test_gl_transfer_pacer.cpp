#include "executor/gpu_work_window.hpp"
#include "executor/opengl/gl_transfer_pacer.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/opengl/gl_protocol_ids.h"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <initializer_list>

using av::GlTransferPacer;
using av::GpuWorkWindow;
using av::WireReader;
using av::WireWriter;
namespace ids = av::glproto;

namespace {

std::uint32_t fences = 0u, waits = 0u, deletes = 0u, flushes = 0u;
int           marker = 0;

GLsync fake_fence(GLenum, GLbitfield)
{
    fences++;
    return reinterpret_cast<GLsync>(&marker);
}
GLenum fake_wait(GLsync, GLbitfield, GLuint64)
{
    waits++;
    return 0x911Au;
}
void fake_delete(GLsync) { deletes++; }
void fake_flush() { flushes++; }

struct Packet {
    std::uint8_t bytes[256] = {};
    WireWriter   writer{bytes, sizeof bytes};
    explicit Packet(std::initializer_list<std::uint64_t> values)
    {
        for (const std::uint64_t value : values) writer.write_varint(value);
    }
    WireReader reader() const { return WireReader(bytes, writer.offset()); }
};

av::glproto::RealGl fake_gl()
{
    av::glproto::RealGl gl{};
    gl.glFenceSync = &fake_fence;
    gl.glClientWaitSync = &fake_wait;
    gl.glDeleteSync = &fake_delete;
    gl.glFlush = &fake_flush;
    return gl;
}

}

TEST(gl_transfer_pacer, what_each_transfer_moves_is_read_from_its_arguments)
{ GlTransferPacer pacer;
  const Packet copy{0x8F36u, 0x8F37u, 0u, 0u, 4096u};
  CHECK_EQ(pacer.cost(ids::kCmd_glCopyBufferSubData, copy.reader()), 4096u);
  const Packet named{1u, 2u, 0u, 0u, 100u};
  CHECK_EQ(pacer.cost(ids::kCmd_glCopyNamedBufferSubData, named.reader()), 100u);
  const Packet image{1u, 0xDE1u, 0u, 0u, 0u, 0u, 2u, 0xDE1u, 0u, 0u, 0u, 0u, 16u, 8u, 2u};
  CHECK_EQ(pacer.cost(ids::kCmd_glCopyImageSubData, image.reader()), 16u * 8u * 2u * 4u);
  const Packet blit{0u, 0u, 64u, 64u, 0u, 0u, 128u, 32u, 0x4000u, 0x2600u};
  CHECK_EQ(pacer.cost(ids::kCmd_glBlitFramebuffer, blit.reader()), 128u * 32u * 4u);
  const Packet flipped{0u, 0u, 64u, 64u, 128u, 32u, 0u, 0u, 0x4000u, 0x2600u};
  CHECK_EQ(pacer.cost(ids::kCmd_glBlitFramebuffer, flipped.reader()), 128u * 32u * 4u);
  const Packet blit_named{1u, 2u, 0u, 0u, 64u, 64u, 0u, 0u, 16u, 16u, 0x4000u, 0x2600u};
  CHECK_EQ(pacer.cost(ids::kCmd_glBlitNamedFramebuffer, blit_named.reader()), 16u * 16u * 4u);
  const Packet clear{1u, 0u, 0u, 0u, 0u, 32u, 32u, 4u};
  CHECK_EQ(pacer.cost(ids::kCmd_glClearTexSubImage, clear.reader()), 32u * 32u * 4u * 4u);
  CHECK_EQ(pacer.cost(ids::kCmd_glDrawArrays, copy.reader()), 0u);
  const Packet truncated{1u};
  CHECK_EQ(pacer.cost(ids::kCmd_glCopyBufferSubData, truncated.reader()), 0u);
  CHECK(pacer.invariants_hold()); }

TEST(gl_transfer_pacer, a_full_window_fences_the_gpu_and_waits_for_the_fence_before_it)
{ GlTransferPacer pacer;
  const av::glproto::RealGl gl = fake_gl();
  fences = waits = deletes = flushes = 0u;
  const Packet big{1u, 2u, 0u, 0u, GpuWorkWindow::window_bytes};
  pacer.ran(gl, ids::kCmd_glCopyNamedBufferSubData, big.reader());
  CHECK_EQ(fences, 1u);
  CHECK_EQ(flushes, 1u);
  CHECK_EQ(waits, 0u);
  const Packet small{1u, 2u, 0u, 0u, 16u};
  pacer.ran(gl, ids::kCmd_glCopyNamedBufferSubData, small.reader());
  CHECK_EQ(fences, 1u);
  pacer.ran(gl, ids::kCmd_glCopyNamedBufferSubData, big.reader());
  CHECK_EQ(fences, 2u);
  CHECK_EQ(waits, 1u);
  CHECK_EQ(deletes, 1u);
  pacer.stop(gl);
  CHECK_EQ(deletes, 2u);
  pacer.stop(gl);
  CHECK_EQ(deletes, 2u); }

TEST(gl_transfer_pacer, a_driver_without_fences_is_not_paced)
{ GlTransferPacer pacer;
  av::glproto::RealGl gl{};
  const Packet big{1u, 2u, 0u, 0u, GpuWorkWindow::window_bytes};
  pacer.ran(gl, ids::kCmd_glCopyNamedBufferSubData, big.reader());
  pacer.stop(gl);
  CHECK(pacer.invariants_hold()); }
