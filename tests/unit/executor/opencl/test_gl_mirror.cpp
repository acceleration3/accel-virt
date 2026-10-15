/* One cl_mem standing in for a GL object (GlMirror): what it says it is, and
 * the turn OpenCL holds between an acquire and a release. */
#include "executor/opencl/gl_mirror.hpp"
#include "harness/framework/test_registry.hpp"

#include <CL/cl_gl.h>
#include <cstdint>

using av::GlLevel;
using av::GlMirror;

namespace {

const void *mem_at(std::uintptr_t at) { return reinterpret_cast<const void *>(at); }

}  // namespace

TEST(gl_mirror, an_empty_mirror_describes_nothing)
{ const GlMirror empty;
  CHECK(!empty.described());
  CHECK(empty.mem() == nullptr);
  CHECK_EQ(empty.object_type(), 0u);
  CHECK(!empty.buffer());
  CHECK(!empty.texture());
  CHECK(!empty.acquired()); }

TEST(gl_mirror, a_buffer_s_mirror_is_its_bytes_and_no_texture)
{ GlMirror mirror(mem_at(0x1000u), 7u, 4096u, true);
  CHECK(mirror.described());
  CHECK(mirror.mem() == mem_at(0x1000u));
  CHECK_EQ(mirror.object_type(), static_cast<std::uint32_t>(CL_GL_OBJECT_BUFFER));
  CHECK(mirror.buffer());
  CHECK(!mirror.texture());
  CHECK_EQ(mirror.name(), 7u);
  CHECK_EQ(mirror.target(), 0u);
  CHECK_EQ(mirror.bytes(), 4096u);
  CHECK(mirror.image() == nullptr);
  CHECK(mirror.written_back());
  /* OpenCL's turn, taken and given back. */
  CHECK(!mirror.acquired());
  mirror.acquire();
  CHECK(mirror.acquired());
  mirror.release();
  CHECK(!mirror.acquired());
  /* Read-only is never copied back; no name, no bytes or no mem is a mirror
   * of nothing. */
  CHECK(!GlMirror(mem_at(0x1000u), 7u, 4096u, false).written_back());
  CHECK(!GlMirror(mem_at(0x1000u), 0u, 4096u, true).described());
  CHECK(!GlMirror(mem_at(0x1000u), 7u, 0u, true).described());
  CHECK(!GlMirror(nullptr, 7u, 4096u, true).described()); }

TEST(gl_mirror, a_texture_s_and_a_renderbuffer_s_mirror_keep_their_level_and_image)
{ GlLevel level(31u, 0x8513u, 0x8517u, 2u, 3);
  level.moves_as(0x1908u, 0x1401u, 8u, 8u, 1u);
  const GlMirror face(mem_at(0x2000u), CL_GL_OBJECT_TEXTURE2D, 31u, 0x8517u, level, 256u, nullptr, false);
  CHECK(face.described());
  CHECK(face.texture());
  CHECK(!face.buffer());
  CHECK_EQ(face.target(), 0x8517u);
  CHECK_EQ(face.level().level(), 3);
  CHECK_EQ(face.level().bind_target(), 0x8513u);
  CHECK_EQ(face.level().image_target(), 0x8517u);
  CHECK_EQ(face.level().dimensions(), 2u);
  CHECK_EQ(face.level().format(), 0x1908u);
  CHECK_EQ(face.level().type(), 0x1401u);
  CHECK_EQ(face.level().width(), 8u);
  CHECK_EQ(face.level().height(), 8u);
  CHECK_EQ(face.level().depth(), 1u);
  CHECK(!face.written_back());

  int            image = 0;
  const GlLevel  over(44u, 0x0DE1u, 0x0DE1u, 2u, 0);
  const GlMirror renderbuffer(mem_at(0x3000u), CL_GL_OBJECT_RENDERBUFFER, 9u, 0u, over, 64u, &image, true);
  CHECK(renderbuffer.described());
  CHECK(!renderbuffer.texture());
  CHECK(!renderbuffer.buffer());
  CHECK_EQ(renderbuffer.name(), 9u);
  CHECK_EQ(renderbuffer.level().texture(), 44u);
  CHECK(renderbuffer.image() == &image);
  /* A type of nothing is a mirror of nothing. */
  CHECK(!GlMirror(mem_at(0x3000u), 0u, 9u, 0u, over, 64u, nullptr, true).described()); }
