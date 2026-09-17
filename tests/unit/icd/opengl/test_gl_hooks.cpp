/* The hooks the generated OpenGL code calls: each answers from the calling
 * thread's current context, and with none - or no root bound at all - answers
 * as if nothing were there. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_icd_rig.hpp"
#include "icd/opengl/gl_icd_binding.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "proto/gl_icd_support.hpp"
#include "proto/icd_stub_report.hpp"

#include "gl_helpers.h"

#include <cstdint>

using av::GlIcdRig;
namespace glicd = av::glicd;
namespace glproto = av::glproto;

namespace {

constexpr GLenum gl_rgba = 0x1908u, gl_unsigned_byte = 0x1401u;
constexpr GLenum pixel_pack_buffer = 0x88EBu, query_buffer = 0x9192u;
constexpr GLenum texture0 = 0x84C0u, texture_2d = 0x0DE1u;
constexpr GLenum max_viewport_dims = 0x0D3Au, uniform_block_binding = 0x8A3Fu;

/* Every hook's answer with nothing to answer from. */
void check_nothing_answers(av::ClientTransport &wire)
{
    CHECK(glicd::wire() == nullptr);
    CHECK_EQ(glicd::pixel_bytes(gl_rgba, gl_unsigned_byte, 2, 2, 1, true), 0u);
    CHECK_EQ(glicd::compressed_bytes(2, 2, 1, 40, false), 0u);
    const void *const offsets[] = {nullptr};
    CHECK(glicd::offsets_of(offsets, 1u) == nullptr);
    CHECK(!glicd::pixel_buffer_bound(true));
    CHECK(!glicd::query_buffer_bound());
    glicd::forget_bindings();
    CHECK(!glicd::skip_active_texture(texture0));
    CHECK(!glicd::skip_bind_texture(texture_2d, 1u));
    CHECK(!glicd::skip_bind_sampler(0u, 1u));
    /* The registry's own count still answers a length query. */
    CHECK_EQ(glproto::pname_count(wire, max_viewport_dims), 2u);
    CHECK_EQ(glproto::uniform_value_count(wire, 1u, 0), 0u);
    CHECK_EQ(glproto::uniform_block_value_count(wire, 1u, 0u, uniform_block_binding), 0u);
    av::icd::payload_dropped("gl", "glNothing", 9u);
}

}  // namespace

TEST(gl_hooks, answer_nothing_with_no_root_bound)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  av::ClientTransport &wire = *glicd::wire();
  glicd::gl_icd_binding.unbind();
  check_nothing_answers(wire);
  CHECK_EQ(glproto::mint_sync_token(), 0u);
  glicd::gl_icd_binding.bind(rig.root());
  CHECK(glicd::wire() == &wire); }

TEST(gl_hooks, answer_nothing_with_nothing_current)
{ GlIcdRig rig;
  rig.load_with_format();
  void *const context = rig.current_context();
  av::ClientTransport &wire = *glicd::wire();
  CHECK(rig.opengl32().make_current(nullptr, nullptr));
  const std::uint32_t lines = rig.log().lines();
  check_nothing_answers(wire);
  /* A dropped payload is logged by a bound root, current context or not. */
  CHECK_EQ(rig.log().lines(), lines + 1u);
  CHECK(glproto::mint_sync_token() != 0u);
  CHECK(rig.opengl32().delete_context(context)); }

TEST(gl_hooks, answer_from_the_current_context)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK(rig.current_context() != nullptr);
  glicd::GuestGlContext &context = *rig.root().current();
  av::ClientTransport &wire = *glicd::wire();
  /* RGBA bytes at the default alignment of 4: 2x2 is 16 bytes. */
  CHECK_EQ(glicd::pixel_bytes(gl_rgba, gl_unsigned_byte, 2, 2, 1, true), 16u);
  CHECK_EQ(glicd::compressed_bytes(2, 2, 1, 40, false), 40u);
  const void *const offsets[] = {reinterpret_cast<const void *>(std::uintptr_t{12u})};
  const std::uint64_t *const wired = glicd::offsets_of(offsets, 1u);
  CHECK(wired != nullptr && wired[0] == 12u);
  CHECK(!glicd::pixel_buffer_bound(true));
  context.arrays().bind_buffer(pixel_pack_buffer, 3u);
  CHECK(glicd::pixel_buffer_bound(true));
  CHECK(!glicd::query_buffer_bound());
  context.arrays().bind_buffer(query_buffer, 4u);
  CHECK(glicd::query_buffer_bound());
  CHECK(!glicd::skip_active_texture(texture0 + 1u));
  CHECK(glicd::skip_active_texture(texture0 + 1u));
  CHECK(!glicd::skip_bind_texture(texture_2d, 5u));
  CHECK(glicd::skip_bind_texture(texture_2d, 5u));
  CHECK(!glicd::skip_bind_sampler(1u, 6u));
  CHECK(glicd::skip_bind_sampler(1u, 6u));
  glicd::forget_bindings();
  CHECK(!glicd::skip_bind_texture(texture_2d, 5u));
  CHECK_EQ(glproto::pname_count(wire, max_viewport_dims), 2u);
  context.uniforms().remember(7u, 2, 0x8B5Cu /* GL_FLOAT_MAT4 */);
  CHECK_EQ(glproto::uniform_value_count(wire, 7u, 2), 16u);
  CHECK_EQ(glproto::uniform_block_value_count(wire, 7u, 0u, uniform_block_binding), 1u); }
