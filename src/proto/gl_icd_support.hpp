/* What the generated OpenGL ICD entries ask of the ICD (feature 050,
 * research.md §2): the calling thread's current context's transport, the pixel
 * store and buffer bindings a transfer's length depends on, the window's
 * framebuffer, and the binding shadow. Defined in src/icd/opengl/gl_hooks.cpp,
 * each one line into the bound GlIcdRoot. */
#ifndef AV_PROTO_GL_ICD_SUPPORT_HPP
#define AV_PROTO_GL_ICD_SUPPORT_HPP

#include "gl_types.h"

#include <stddef.h>
#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::glicd {

/* Null when the thread has no current context: the call does nothing. */
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
/* What changes a buffer's bytes on the host, for the guest's copies of them
 * (rvk::glicd::BufferShadows): before the command is sent. */
void             wrote_buffer(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) noexcept;
void             wrote_named_buffer(GLuint buffer, GLintptr offset, GLsizeiptr size, const void *data) noexcept;
void             host_writes_target(GLenum target) noexcept;
void             host_writes_buffer(GLuint buffer) noexcept;
void             bind_buffer_indexed(GLenum target, GLuint buffer) noexcept;
void             bind_buffers_indexed(GLenum target) noexcept;
void             shaders_write_buffers() noexcept;
void             bindings_restored() noexcept;
/* A buffer's map state, which only the guest holds: true once `params` has
 * the answer, false for a name the host answers. */
bool             answer_buffer_map_state(GLenum target, GLenum pname, GLint *params) noexcept;
bool             answer_buffer_map_state(GLenum target, GLenum pname, GLint64 *params) noexcept;
bool             answer_named_buffer_map_state(GLuint buffer, GLenum pname, GLint *params) noexcept;
bool             answer_named_buffer_map_state(GLuint buffer, GLenum pname, GLint64 *params) noexcept;
/* Whether the current context has the fixed-function pipeline: a compatibility
 * profile, or a context made before profiles existed (feature 051). A
 * compatibility command called without one is refused rather than sent. */
bool             legacy_calls_live() noexcept;
void             refuse_legacy() noexcept;

}  // namespace av::glicd

#endif  // AV_PROTO_GL_ICD_SUPPORT_HPP
