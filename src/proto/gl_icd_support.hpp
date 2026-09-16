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

}  // namespace av::glicd

#endif  // AV_PROTO_GL_ICD_SUPPORT_HPP
