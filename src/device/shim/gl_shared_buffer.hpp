/* A GL buffer over an imported memory object, so OpenCL can name it
 * (feature 053, T019).
 *
 * This host's OpenCL has no `cl_khr_external_memory`, so a shared object
 * cannot be handed to it as a descriptor. It does have `cl_khr_gl_sharing`,
 * and OpenGL takes the descriptor - which this project already proves on every
 * GPU it runs on. So the descriptor becomes a GL memory object
 * (`GlHostImport`), this puts a buffer over that memory, and
 * `clCreateFromGLBuffer` names the buffer. One allocation throughout: what
 * Vulkan wrote, an OpenCL kernel reads.
 *
 * The buffer is storage and nothing else. Nothing ever draws with it, which is
 * what lets OpenCL hold it acquired for its whole life rather than bracketing
 * every enqueue - the acquire is a rule about GL and CL taking turns, and GL
 * never takes one.
 *
 * Every call makes the context current itself, as GlHostImport's do, so
 * nothing above has to ask whether a closing context still has one. */
#ifndef AV_GL_SHARED_BUFFER_HPP
#define AV_GL_SHARED_BUFFER_HPP

#include "proto/generated/opengl/gl_types.h"

#include <stdint.h>

namespace av {

class GlContext;

class GlSharedBuffer {
public:
    GlSharedBuffer();
    GlSharedBuffer(const GlSharedBuffer &) = delete;
    GlSharedBuffer &operator=(const GlSharedBuffer &) = delete;

    /* Keeps `context`, which must outlive this, and resolves the four entry
     * points a buffer over imported memory needs. A host missing any of them
     * leaves this making nothing, which the caller answers as a refusal. */
    void configure(GlContext &context);
    void give_back();
    /* Whether everything a buffer needs resolved. */
    bool makes_buffers() const;

    /* `size` bytes of the memory object `memory` as a buffer, named in
     * `buffer`. False, with nothing made, for a host that cannot or a driver
     * that refuses - which is what a memory object of the wrong size is. */
    [[nodiscard]] bool make(uint32_t memory, uint64_t size, uint32_t &buffer);
    void               destroy(uint32_t buffer);

private:
    GlContext *current();

    void (APIENTRY *gen_buffers_)(GLsizei, GLuint *);
    void (APIENTRY *bind_buffer_)(GLenum, GLuint);
    void (APIENTRY *buffer_storage_mem_)(GLenum, GLsizeiptr, GLuint, GLuint64);
    void (APIENTRY *delete_buffers_)(GLsizei, const GLuint *);
    GLenum (APIENTRY *get_error_)();
    GlContext *context_;
};

}  // namespace av

#endif  // AV_GL_SHARED_BUFFER_HPP
