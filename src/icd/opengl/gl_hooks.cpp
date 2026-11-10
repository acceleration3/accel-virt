#include "core/trace/function_scope.hpp"
#include "icd/opengl/client_array_state.hpp"
#include "icd/opengl/gl_entry_points.hpp"
#include "icd/opengl/gl_error_query.hpp"
#include "icd/opengl/gl_icd_binding.hpp"
#include "icd/opengl/gl_icd_root.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "icd/opengl/manual_calls.hpp"
#include "proto/client/gl_icd_support.hpp"
#include "proto/client/icd_stub_report.hpp"

#include "gl_dispatch_client.h"
#include "gl_helpers.h"
#include "gl_sizes.h"

namespace av::glicd {

constinit GlIcdBinding gl_icd_binding;

namespace {
GuestGlContext *current_context()
{
    AV_SCOPE("current_context");
    return gl_icd_binding.bound() ? gl_icd_binding.root().current() : nullptr;
}

ClientArrayState *arrays()
{
    AV_SCOPE("arrays");
    GuestGlContext *const context = current_context();
    return context != nullptr ? &context->arrays() : nullptr;
}

}

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
    GuestGlContext *const context = current_context();
    if (context == nullptr) return nullptr;
    const uint64_t *const converted = context->arrays().offsets_of(offsets, count);
    if (converted == nullptr && offsets != nullptr) context->record(GuestGlContext::gl_out_of_memory);
    return converted;
}

const int64_t *wide_intptrs(const GLintptr *values, size_t count, uint32_t which) noexcept
{
    AV_SCOPE("wide_intptrs");
    GuestGlContext *const context = current_context();
    if (context == nullptr) return nullptr;
    const int64_t *const wide = context->arrays().widened(values, count, which);
    if (wide == nullptr && values != nullptr) context->record(GuestGlContext::gl_out_of_memory);
    return wide;
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

GLuint framebuffer_name(GLuint name) noexcept
{
    AV_SCOPE("framebuffer_name");
    GuestGlContext *const context = current_context();
    return context != nullptr ? context->framebuffers().name_of(name) : name;
}

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

bool stage_upload(const void *pixels, size_t bytes) noexcept
{
    AV_SCOPE("stage_upload");
    return gl_icd_binding.bound() && gl_icd_binding.root().manual().stage_upload(pixels, bytes);
}

void finish_upload() noexcept
{
    AV_SCOPE("finish_upload");
    if (gl_icd_binding.bound()) gl_icd_binding.root().manual().finish_upload();
}

bool stage_download(const void *pixels, size_t bytes) noexcept
{
    AV_SCOPE("stage_download");
    return gl_icd_binding.bound() && gl_icd_binding.root().manual().stage_download(pixels, bytes);
}

void finish_download(void *pixels, size_t bytes) noexcept
{
    AV_SCOPE("finish_download");
    if (gl_icd_binding.bound()) gl_icd_binding.root().manual().finish_download(pixels, bytes);
}

void host_writes_target(GLenum target) noexcept
{
    AV_SCOPE("host_writes_target");
    if (gl_icd_binding.bound()) gl_icd_binding.root().manual().host_writes_target(target);
}

void host_writes_buffer(GLuint buffer) noexcept
{
    AV_SCOPE("host_writes_buffer");
    if (gl_icd_binding.bound()) gl_icd_binding.root().manual().host_writes_buffer(buffer);
}

void bind_buffer_indexed(GLenum target, GLuint buffer) noexcept
{
    AV_SCOPE("bind_buffer_indexed");
    if (gl_icd_binding.bound()) gl_icd_binding.root().manual().bind_buffer_indexed(target, buffer);
}

void bind_buffers_indexed(GLenum target) noexcept
{
    AV_SCOPE("bind_buffers_indexed");
    if (gl_icd_binding.bound()) gl_icd_binding.root().manual().bind_buffers_indexed(target);
}

void shaders_write_buffers() noexcept
{
    AV_SCOPE("shaders_write_buffers");
    if (gl_icd_binding.bound()) gl_icd_binding.root().manual().shaders_write_buffers();
}

void client_attributes_pushed(GLbitfield mask) noexcept
{
    AV_SCOPE("client_attributes_pushed");
    if (gl_icd_binding.bound()) gl_icd_binding.root().manual().client_attributes_pushed(mask);
}

void client_attributes_popped() noexcept
{
    AV_SCOPE("client_attributes_popped");
    if (gl_icd_binding.bound()) gl_icd_binding.root().manual().client_attributes_popped();
}

void client_attributes_reset(GLbitfield mask) noexcept
{
    AV_SCOPE("client_attributes_reset");
    if (gl_icd_binding.bound()) gl_icd_binding.root().manual().client_attributes_reset(mask);
}

void vertex_array_elements(GLuint array, GLuint buffer) noexcept
{
    AV_SCOPE("vertex_array_elements");
    if (gl_icd_binding.bound()) gl_icd_binding.root().manual().vertex_array_elements(array, buffer);
}

bool answer_buffer_map_state(GLenum target, GLenum pname, GLint *params) noexcept
{
    AV_SCOPE("answer_buffer_map_state");
    int64_t value = 0;
    if (!gl_icd_binding.bound() || !gl_icd_binding.root().manual().buffer_map_state(target, pname, value))
        return false;
    if (params != nullptr) *params = static_cast<GLint>(value);
    return true;
}

bool answer_buffer_map_state(GLenum target, GLenum pname, GLint64 *params) noexcept
{
    AV_SCOPE("answer_buffer_map_state");
    int64_t value = 0;
    if (!gl_icd_binding.bound() || !gl_icd_binding.root().manual().buffer_map_state(target, pname, value))
        return false;
    if (params != nullptr) *params = value;
    return true;
}

bool answer_named_buffer_map_state(GLuint buffer, GLenum pname, GLint *params) noexcept
{
    AV_SCOPE("answer_named_buffer_map_state");
    int64_t value = 0;
    if (!gl_icd_binding.bound() || !gl_icd_binding.root().manual().named_buffer_map_state(buffer, pname, value))
        return false;
    if (params != nullptr) *params = static_cast<GLint>(value);
    return true;
}

bool answer_named_buffer_map_state(GLuint buffer, GLenum pname, GLint64 *params) noexcept
{
    AV_SCOPE("answer_named_buffer_map_state");
    int64_t value = 0;
    if (!gl_icd_binding.bound() || !gl_icd_binding.root().manual().named_buffer_map_state(buffer, pname, value))
        return false;
    if (params != nullptr) *params = value;
    return true;
}

bool unpack_slice_bound() noexcept
{
    AV_SCOPE("unpack_slice_bound");
    return gl_icd_binding.bound() && gl_icd_binding.root().manual().unpack_slice_bound();
}

bool slice_unpack_pixels(const void *&pixels, size_t bytes) noexcept
{
    AV_SCOPE("slice_unpack_pixels");
    return gl_icd_binding.bound() && gl_icd_binding.root().manual().slice_unpack_pixels(pixels, bytes);
}

void unpack_slice_done() noexcept
{
    AV_SCOPE("unpack_slice_done");
    if (gl_icd_binding.bound()) gl_icd_binding.root().manual().unpack_slice_done();
}

bool pack_slice_bound() noexcept
{
    AV_SCOPE("pack_slice_bound");
    return gl_icd_binding.bound() && gl_icd_binding.root().manual().pack_slice_bound();
}

bool slice_pack_pixels(void *&pixels, size_t bytes) noexcept
{
    AV_SCOPE("slice_pack_pixels");
    return gl_icd_binding.bound() && gl_icd_binding.root().manual().slice_pack_pixels(pixels, bytes);
}

void pack_slice_done() noexcept
{
    AV_SCOPE("pack_slice_done");
    if (gl_icd_binding.bound()) gl_icd_binding.root().manual().pack_slice_done();
}

bool refuse_sliced_unpack() noexcept
{
    AV_SCOPE("refuse_sliced_unpack");
    return gl_icd_binding.bound() && gl_icd_binding.root().manual().refuse_sliced_unpack();
}

bool refuse_sliced_pack() noexcept
{
    AV_SCOPE("refuse_sliced_pack");
    return gl_icd_binding.bound() && gl_icd_binding.root().manual().refuse_sliced_pack();
}

bool clear_bound_buffer(GLenum target, GLenum internalformat, GLenum format, GLenum type, const void *data) noexcept
{
    AV_SCOPE("clear_bound_buffer");
    return gl_icd_binding.bound() && gl_icd_binding.root().manual().clear_bound_buffer(target, internalformat, format, type, data);
}

bool clear_bound_buffer_range(GLenum target, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format,
                              GLenum type, const void *data) noexcept
{
    AV_SCOPE("clear_bound_buffer_range");
    return gl_icd_binding.bound() && gl_icd_binding.root().manual().clear_bound_buffer_range(target, internalformat, offset, size, format, type, data);
}

bool clear_named_buffer(GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void *data) noexcept
{
    AV_SCOPE("clear_named_buffer");
    return gl_icd_binding.bound() && gl_icd_binding.root().manual().clear_named_buffer(buffer, internalformat, format, type, data);
}

bool clear_named_buffer_range(GLuint buffer, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format,
                              GLenum type, const void *data) noexcept
{
    AV_SCOPE("clear_named_buffer_range");
    return gl_icd_binding.bound() && gl_icd_binding.root().manual().clear_named_buffer_range(buffer, internalformat, offset, size, format, type, data);
}

bool invalidate_named_buffer(GLuint buffer) noexcept
{
    AV_SCOPE("invalidate_named_buffer");
    return gl_icd_binding.bound() && gl_icd_binding.root().manual().invalidate_named_buffer(buffer);
}

bool invalidate_named_buffer_range(GLuint buffer, GLintptr offset, GLsizeiptr length) noexcept
{
    AV_SCOPE("invalidate_named_buffer_range");
    return gl_icd_binding.bound() && gl_icd_binding.root().manual().invalidate_named_buffer_range(buffer, offset, length);
}

bool stage_client_arrays(GLint first, GLsizei count) noexcept
{
    AV_SCOPE("stage_client_arrays");
    return !gl_icd_binding.bound() || gl_icd_binding.root().manual().stage_arrays(first, count);
}

bool stage_client_elements(GLenum type, GLsizei count, const void *&indices, GLint base_vertex) noexcept
{
    AV_SCOPE("stage_client_elements");
    return !gl_icd_binding.bound() || gl_icd_binding.root().manual().stage_elements(type, count, indices, base_vertex);
}

bool stage_client_multi_arrays(const GLint *first, const GLsizei *count, GLsizei draws) noexcept
{
    AV_SCOPE("stage_client_multi_arrays");
    return !gl_icd_binding.bound() || gl_icd_binding.root().manual().stage_multi_arrays(first, count, draws);
}

bool stage_client_multi_elements(const GLsizei *count, GLenum type, const void *const *indices, GLsizei draws,
                                 const GLint *base_vertex, const uint64_t *&host_indices) noexcept
{
    AV_SCOPE("stage_client_multi_elements");
    host_indices = nullptr;
    return !gl_icd_binding.bound() ||
           gl_icd_binding.root().manual().stage_multi_elements(count, type, indices, draws, base_vertex, host_indices);
}

bool refuse_client_arrays() noexcept
{
    AV_SCOPE("refuse_client_arrays");
    return gl_icd_binding.bound() && gl_icd_binding.root().manual().refuse_indirect_draw();
}

void finish_client_draw() noexcept
{
    AV_SCOPE("finish_client_draw");
    if (gl_icd_binding.bound()) gl_icd_binding.root().manual().finish_client_draw();
}

void begin_client_primitives() noexcept
{
    AV_SCOPE("begin_client_primitives");
    if (gl_icd_binding.bound()) gl_icd_binding.root().manual().begin_primitives();
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

}

namespace av::glproto {
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

size_t map_point_values(GLenum target) noexcept
{
    AV_SCOPE("map_point_values");
    switch (target) {
    case 0x0D91: case 0x0DB1: case 0x0D93: case 0x0DB3: return 1u;
    case 0x0D94: case 0x0DB4: return 2u;
    case 0x0D92: case 0x0DB2: case 0x0D95: case 0x0DB5: case 0x0D97: case 0x0DB7: return 3u;
    case 0x0D90: case 0x0DB0: case 0x0D96: case 0x0DB6: case 0x0D98: case 0x0DB8: return 4u;
    default: return 0u;
    }
}
size_t map_value_count(ClientTransport &, GLenum target, GLenum query) noexcept
{
    AV_SCOPE("map_value_count");
    const bool two_dimensional = target >= 0x0DB0u && target <= 0x0DB8u;
    if (map_point_values(target) == 0u) return 0u;
    if (query == 0x0A01u) return two_dimensional ? 2u : 1u;
    if (query == 0x0A02u) return two_dimensional ? 4u : 2u;
    return 0u;
}
size_t pixel_map_size(ClientTransport &, GLenum) noexcept
{
    AV_SCOPE("pixel_map_size");
    return 0u;
}
size_t stipple_bytes(ClientTransport &, bool packing) noexcept
{
    AV_SCOPE("stipple_bytes");
    const glicd::ClientArrayState *const state = glicd::arrays();
    return state != nullptr ? state->bitmap_bytes(32, 32, packing) : 0u;
}
size_t bitmap_bytes(ClientTransport &, GLsizei width, GLsizei height) noexcept
{
    AV_SCOPE("bitmap_bytes");
    const glicd::ClientArrayState *const state = glicd::arrays();
    return state != nullptr ? state->bitmap_bytes(width, height, false) : 0u;
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

GLenum av_glGetError(ClientTransport &av_t)
{
    AV_SCOPE("av_glGetError");
    return glicd::GlErrorQuery(av_t).ask();
}

}

namespace av::icd {

void payload_dropped(const char *api, const char *command, size_t written) noexcept
{
    AV_SCOPE("payload_dropped");
    if (glicd::gl_icd_binding.bound()) glicd::gl_icd_binding.root().note_dropped(api, command, written);
}

}
