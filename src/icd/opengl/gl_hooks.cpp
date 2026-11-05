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
