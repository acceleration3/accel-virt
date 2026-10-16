/* A shared descriptor as a GL buffer: the descriptor imported as a memory
 * object of the context (GlHostImport), and a buffer whose storage is that
 * memory. One allocation throughout, whoever names the buffer:
 *
 * - OpenCL, whose host has no `cl_khr_external_memory`, so a shared object
 *   cannot be handed to it as a descriptor; it has `cl_khr_gl_sharing`, and
 *   `clCreateFromGLBuffer` names the buffer made in its vehicle (feature 053,
 *   T019). Nothing ever draws with it, which is what lets OpenCL hold it
 *   acquired for its whole life.
 * - An OpenGL context's pixel transfers, packed by the GPU into the guest's
 *   region the buffer is made over (GlPackBuffer).
 *
 * Made with direct state access, so no binding of the context changes: in an
 * OpenGL backend's context every binding is the guest's. A storage the driver
 * refused is judged by the buffer's size, not by glGetError, whose flags in
 * that context are the guest's errors too.
 *
 * Every call makes the context current itself, as GlHostImport's do, so
 * nothing above has to ask whether a closing context still has one. */
#ifndef AV_GL_SHARED_BUFFER_HPP
#define AV_GL_SHARED_BUFFER_HPP

#include "executor/shim/gl_error_flags.hpp"
#include "proto/generated/opengl/gl_types.h"

#include <stdint.h>

namespace av {

class GlContext;
class GlHostImport;

class GlSharedBuffer {
public:
    GlSharedBuffer();
    GlSharedBuffer(const GlSharedBuffer &) = delete;
    GlSharedBuffer &operator=(const GlSharedBuffer &) = delete;

    /* Keeps `context` and `import`, configured on that context, which must
     * outlive this, and resolves the entry points a buffer over imported
     * memory needs. A host missing any of them leaves this making nothing,
     * which the caller answers as a refusal. */
    void configure(GlContext &context, GlHostImport &import);
    void give_back();
    /* Whether the context imports memory and everything a buffer over it
     * needs resolved. */
    bool makes_buffers() const;

    /* `size` bytes of the dma-buf `fd` as a memory object and a buffer over
     * it, named in `memory` and `buffer`; `fd` stays the caller's. False, with
     * nothing made, for a host that cannot or a driver that refuses - which is
     * what a descriptor smaller than `size` is. */
    [[nodiscard]] bool make(int fd, uint64_t size, uint32_t &memory, uint32_t &buffer);
    /* What make made: the buffer, then the memory under it. */
    void               destroy(uint32_t memory, uint32_t buffer);

private:
    GlContext *current();

    void (APIENTRY *create_buffers_)(GLsizei, GLuint *);
    void (APIENTRY *storage_mem_)(GLuint, GLsizeiptr, GLuint, GLuint64);
    void (APIENTRY *buffer_parameter_)(GLuint, GLenum, GLint64 *);
    void (APIENTRY *delete_buffers_)(GLsizei, const GLuint *);
    GlErrorFlags errors_;
    GlContext    *context_;
    GlHostImport *import_;
};

}  // namespace av

#endif  // AV_GL_SHARED_BUFFER_HPP
