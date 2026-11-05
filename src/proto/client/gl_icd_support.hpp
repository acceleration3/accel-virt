#ifndef AV_PROTO_GL_ICD_SUPPORT_HPP
#define AV_PROTO_GL_ICD_SUPPORT_HPP

#include "gl_types.h"

#include <stddef.h>
#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::glicd {
ClientTransport *wire() noexcept;
size_t           pixel_bytes(GLenum format, GLenum type, GLsizei width, GLsizei height, GLsizei depth,
                             bool packing) noexcept;
size_t           compressed_bytes(GLsizei width, GLsizei height, GLsizei depth, GLsizei image_size,
                                  bool packing) noexcept;
size_t           level_bytes(GLenum target, GLint level, GLenum format, GLenum type) noexcept;
size_t           compressed_level_bytes(GLenum target, GLint level, bool packing) noexcept;
const uint64_t  *offsets_of(const void *const *offsets, size_t count) noexcept;
bool             pixel_buffer_bound(bool packing) noexcept;
bool             query_buffer_bound() noexcept;
GLuint           framebuffer_name(GLuint name) noexcept;
void             flush_persistent_mappings() noexcept;
void             forget_bindings() noexcept;
bool             skip_active_texture(GLenum texture) noexcept;
bool             skip_bind_texture(GLenum target, GLuint texture) noexcept;
bool             skip_bind_sampler(GLuint unit, GLuint sampler) noexcept;
bool             stage_upload(const void *pixels, size_t bytes) noexcept;
void             finish_upload() noexcept;
bool             stage_download(const void *pixels, size_t bytes) noexcept;
void             finish_download(void *pixels, size_t bytes) noexcept;
void             host_writes_target(GLenum target) noexcept;
void             host_writes_buffer(GLuint buffer) noexcept;
void             bind_buffer_indexed(GLenum target, GLuint buffer) noexcept;
void             bind_buffers_indexed(GLenum target) noexcept;
void             shaders_write_buffers() noexcept;
void             client_attributes_pushed(GLbitfield mask) noexcept;
void             client_attributes_popped() noexcept;
void             client_attributes_reset(GLbitfield mask) noexcept;
void             vertex_array_elements(GLuint array, GLuint buffer) noexcept;
bool             answer_buffer_map_state(GLenum target, GLenum pname, GLint *params) noexcept;
bool             answer_buffer_map_state(GLenum target, GLenum pname, GLint64 *params) noexcept;
bool             answer_named_buffer_map_state(GLuint buffer, GLenum pname, GLint *params) noexcept;
bool             answer_named_buffer_map_state(GLuint buffer, GLenum pname, GLint64 *params) noexcept;
bool             legacy_calls_live() noexcept;
void             refuse_legacy() noexcept;

}

#endif
