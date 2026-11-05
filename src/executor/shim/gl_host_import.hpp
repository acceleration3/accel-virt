#ifndef AV_GL_HOST_IMPORT_HPP
#define AV_GL_HOST_IMPORT_HPP

#include "executor/shim/gl_error_flags.hpp"
#include "proto/generated/opengl/gl_types.h"

#include <stdint.h>

namespace av {

class GlContext;

class GlHostImport {
public:
    GlHostImport();
    GlHostImport(const GlHostImport &) = delete;
    GlHostImport &operator=(const GlHostImport &) = delete;

    void configure(GlContext &context);
    void give_back();
    bool imports_memory() const;
    bool places_buffers() const;

    [[nodiscard]] bool import_memory(int fd, uint64_t size, uint32_t &name);
    [[nodiscard]] bool import_dedicated_memory(int fd, uint64_t size, uint32_t &name);
    [[nodiscard]] bool import_semaphore(int fd, uint32_t &name);
    void               delete_memory(uint32_t name);
    void               delete_semaphore(uint32_t name);

private:
    bool lists(const char *extension) const;
    bool names_buffers() const;
    bool import_with(int fd, uint64_t size, GLint dedicated, uint32_t &name);
    GlContext *current();

    void (APIENTRY *create_memory_objects_)(GLsizei, GLuint *);
    void (APIENTRY *import_memory_fd_)(GLuint, GLuint64, GLenum, GLint);
    void (APIENTRY *memory_parameter_)(GLuint, GLenum, const GLint *);
    void (APIENTRY *delete_memory_objects_)(GLsizei, const GLuint *);
    void (APIENTRY *gen_semaphores_)(GLsizei, GLuint *);
    void (APIENTRY *import_semaphore_fd_)(GLuint, GLenum, GLint);
    void (APIENTRY *delete_semaphores_)(GLsizei, const GLuint *);
    void (APIENTRY *get_integerv_)(GLenum, GLint *);
    const GLubyte *(APIENTRY *get_stringi_)(GLenum, GLuint);
    GlErrorFlags errors_;
    GlContext *context_;
    bool memory_;
    bool buffers_;
    bool fences_;
};

}

#endif
