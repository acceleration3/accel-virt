/* The hooks the generated OpenGL entries and client call by name (gl_icd_support
 * and gl_helpers.h), each one line into the bound root or the calling thread's
 * context, and the binding they reach it through - the OpenGL ICD's process
 * state (constitution III). A hook asked with nothing current answers as if
 * nothing were bound: no wire, no bytes, nothing to skip. */
#include "core/trace/function_scope.hpp"
#include "icd/opengl/client_array_state.hpp"
#include "icd/opengl/gl_entry_points.hpp"
#include "icd/opengl/gl_error_query.hpp"
#include "icd/opengl/gl_icd_binding.hpp"
#include "icd/opengl/gl_icd_root.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "icd/opengl/manual_calls.hpp"
#include "proto/gl_icd_support.hpp"
#include "proto/icd_stub_report.hpp"

#include "gl_dispatch_client.h"
#include "gl_helpers.h"
#include "gl_sizes.h"

namespace av::glicd {

constinit GlIcdBinding gl_icd_binding;

namespace {

/* The calling thread's context, or null - also with no root bound, as for a
 * transport a test drives without one. */
GuestGlContext *current_context()
{
    AV_SCOPE("current_context");
    return gl_icd_binding.bound() ? gl_icd_binding.root().current() : nullptr;
}

/* The calling thread's context's client state, or null. */
ClientArrayState *arrays()
{
    AV_SCOPE("arrays");
    GuestGlContext *const context = current_context();
    return context != nullptr ? &context->arrays() : nullptr;
}

}  // namespace

ClientTransport *wire() noexcept
{
    AV_SCOPE("wire");
    return gl_icd_binding.bound() ? gl_icd_binding.root().wire() : nullptr;
}

size_t pixel_bytes(GLenum format, GLenum type, GLsizei width, GLsizei height, GLsizei depth, bool packing) noexcept
{
    AV_SCOPE("pixel_bytes");
    const ClientArrayState *const state = arrays();
    return state != nullptr ? state->pixel_bytes(format, type, width, height, depth, packing) : 0u;
}

size_t compressed_bytes(GLsizei, GLsizei, GLsizei, GLsizei image_size, bool packing) noexcept
{
    AV_SCOPE("compressed_bytes");
    const ClientArrayState *const state = arrays();
    return state != nullptr ? state->compressed_bytes(image_size, packing) : 0u;
}

/* A level's size is the host's, and no hook may ask it inside a call: a
 * download sized from a level answers nothing (research.md §5). */
/* A whole level's download is sized by the host's answer for the level: the
 * host refuses one whose buffer is short of it. */
size_t level_bytes(GLenum target, GLint level, GLenum format, GLenum type) noexcept
{
    AV_SCOPE("level_bytes");
    return gl_icd_binding.root().manual().level_bytes(target, level, format, type);
}
size_t compressed_level_bytes(GLenum target, GLint level, bool) noexcept
{
    AV_SCOPE("compressed_level_bytes");
    return gl_icd_binding.root().manual().compressed_level_bytes(target, level);
}

const uint64_t *offsets_of(const void *const *offsets, size_t count) noexcept
{
    AV_SCOPE("offsets_of");
    ClientArrayState *const state = arrays();
    return state != nullptr ? state->offsets_of(offsets, count) : nullptr;
}

bool pixel_buffer_bound(bool packing) noexcept
{
    AV_SCOPE("pixel_buffer_bound");
    const ClientArrayState *const state = arrays();
    return state != nullptr && state->pixel_buffer_bound(packing);
}

bool query_buffer_bound() noexcept
{
    AV_SCOPE("query_buffer_bound");
    const ClientArrayState *const state = arrays();
    return state != nullptr && state->query_buffer_bound();
}

/* Framebuffer 0 is the calling context's current window. */
GLuint framebuffer_name(GLuint name) noexcept
{
    AV_SCOPE("framebuffer_name");
    GuestGlContext *const context = current_context();
    return context != nullptr ? context->framebuffers().name_of(name) : name;
}

/* What a persistent map wrote goes to the host before a command reads it. */
void flush_persistent_mappings() noexcept
{
    AV_SCOPE("flush_persistent_mappings");
    GuestGlContext *const context = current_context();
    ClientTransport *const wire = context != nullptr ? context->wire() : nullptr;
    if (wire != nullptr) context->mappings().flush_persistent(*wire);
}

void forget_bindings() noexcept
{
    AV_SCOPE("forget_bindings");
    ClientArrayState *const state = arrays();
    if (state != nullptr) state->forget_bindings();
}

bool skip_active_texture(GLenum texture) noexcept
{
    AV_SCOPE("skip_active_texture");
    ClientArrayState *const state = arrays();
    return state != nullptr && state->skip_active_texture(texture);
}

bool skip_bind_texture(GLenum target, GLuint texture) noexcept
{
    AV_SCOPE("skip_bind_texture");
    ClientArrayState *const state = arrays();
    return state != nullptr && state->skip_bind_texture(target, texture);
}

bool skip_bind_sampler(GLuint unit, GLuint sampler) noexcept
{
    AV_SCOPE("skip_bind_sampler");
    ClientArrayState *const state = arrays();
    return state != nullptr && state->skip_bind_sampler(unit, sampler);
}

bool legacy_calls_live() noexcept
{
    AV_SCOPE("legacy_calls_live");
    const GuestGlContext *const context = current_context();
    return context != nullptr && context->profile().legacy_calls_live();
}

void refuse_legacy() noexcept
{
    AV_SCOPE("refuse_legacy");
    static_cast<void>(gl_icd_binding.root().record_legacy());
}

}  // namespace av::glicd

namespace av::glproto {

/* With nothing current - a context asking its own list lengths as it is
 * made - only the registry's counts are known, and those are all a length
 * query needs. */
size_t pname_count(ClientTransport &, uint32_t pname) noexcept
{
    AV_SCOPE("pname_count");
    glicd::GuestGlContext *const context = glicd::current_context();
    return context != nullptr ? context->queries().values(pname) : glproto::pname_count(pname);
}

size_t uniform_value_count(ClientTransport &, GLuint program, GLint location) noexcept
{
    AV_SCOPE("uniform_value_count");
    glicd::GuestGlContext *const context = glicd::current_context();
    return context != nullptr ? context->uniforms().values(program, location) : 0u;
}

size_t uniform_block_value_count(ClientTransport &, GLuint, GLuint, GLenum pname) noexcept
{
    AV_SCOPE("uniform_block_value_count");
    glicd::GuestGlContext *const context = glicd::current_context();
    return context != nullptr ? context->queries().block_values(pname) : 0u;
}

/* The evaluator, pixel map, stipple, bitmap and ARB program queries are the
 * compatibility profile's (051): their lengths are not kept, so nothing is
 * written. */
size_t map_value_count(ClientTransport &, GLenum, GLenum) noexcept
{
    AV_SCOPE("map_value_count");
    return 0u;
}
size_t map_point_values(GLenum) noexcept
{
    AV_SCOPE("map_point_values");
    return 0u;
}
size_t pixel_map_size(ClientTransport &, GLenum) noexcept
{
    AV_SCOPE("pixel_map_size");
    return 0u;
}
size_t stipple_bytes(ClientTransport &, bool) noexcept
{
    AV_SCOPE("stipple_bytes");
    return 0u;
}
size_t bitmap_bytes(ClientTransport &, GLsizei, GLsizei) noexcept
{
    AV_SCOPE("bitmap_bytes");
    return 0u;
}
size_t named_program_string_bytes(ClientTransport &, GLuint, GLenum) noexcept
{
    AV_SCOPE("named_program_string_bytes");
    return 0u;
}

uint64_t mint_sync_token() noexcept
{
    AV_SCOPE("mint_sync_token");
    return glicd::gl_icd_binding.bound() ? glicd::gl_icd_binding.root().mint_sync_token() : 0u;
}

GLenum rvk_glGetError(ClientTransport &rvk_t)
{
    AV_SCOPE("rvk_glGetError");
    return glicd::GlErrorQuery(rvk_t).ask();
}

}  // namespace av::glproto

namespace av::icd {

void payload_dropped(const char *api, const char *command, size_t written) noexcept
{
    AV_SCOPE("payload_dropped");
    if (glicd::gl_icd_binding.bound()) glicd::gl_icd_binding.root().note_dropped(api, command, written);
}

}  // namespace av::icd
