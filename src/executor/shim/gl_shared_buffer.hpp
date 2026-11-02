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

    void configure(GlContext &context, GlHostImport &import);
    void give_back();
    bool makes_buffers() const;

    [[nodiscard]] bool make(int fd, uint64_t size, uint32_t &memory, uint32_t &buffer);
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

}

#endif
