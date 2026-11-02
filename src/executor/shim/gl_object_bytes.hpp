#ifndef AV_GL_OBJECT_BYTES_HPP
#define AV_GL_OBJECT_BYTES_HPP

#include "executor/shim/gl_error_flags.hpp"
#include "proto/generated/opengl/gl_types.h"

#include <stdint.h>

namespace av {

class GlContext;
class GlLevel;

class GlObjectBytes {
public:
    GlObjectBytes();
    GlObjectBytes(const GlObjectBytes &) = delete;
    GlObjectBytes &operator=(const GlObjectBytes &) = delete;

    void configure(GlContext &context);
    void give_back();
    bool moves_bytes() const;

    [[nodiscard]] bool buffer_size(uint32_t buffer, uint64_t &bytes);
    [[nodiscard]] bool read_buffer(uint32_t buffer, uint64_t bytes, void *into);
    [[nodiscard]] bool write_buffer(uint32_t buffer, uint64_t bytes, const void *from);
    [[nodiscard]] bool level_shape(const GlLevel &level, uint32_t &internal_format, uint32_t &width,
                                   uint32_t &height, uint32_t &depth);
    [[nodiscard]] bool read_level(const GlLevel &level, void *into);
    [[nodiscard]] bool write_level(const GlLevel &level, const void *from);
    void               finish();

private:
    GlContext *current();
    bool       quiet();
    [[nodiscard]] bool bound(uint32_t target, uint32_t texture);

    GLboolean (APIENTRY *is_buffer_)(GLuint);
    void (APIENTRY *bind_buffer_)(GLenum, GLuint);
    void (APIENTRY *get_buffer_parameter_)(GLenum, GLenum, GLint64 *);
    void (APIENTRY *get_buffer_sub_data_)(GLenum, GLintptr, GLsizeiptr, void *);
    void (APIENTRY *buffer_sub_data_)(GLenum, GLintptr, GLsizeiptr, const void *);
    GLboolean (APIENTRY *is_texture_)(GLuint);
    void (APIENTRY *bind_texture_)(GLenum, GLuint);
    void (APIENTRY *get_level_parameter_)(GLenum, GLint, GLenum, GLint *);
    void (APIENTRY *get_tex_image_)(GLenum, GLint, GLenum, GLenum, void *);
    void (APIENTRY *tex_sub_image_1d_)(GLenum, GLint, GLint, GLsizei, GLenum, GLenum, const void *);
    void (APIENTRY *tex_sub_image_2d_)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *);
    void (APIENTRY *tex_sub_image_3d_)(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum,
                                       GLenum, const void *);
    void (APIENTRY *pixel_store_)(GLenum, GLint);
    GlErrorFlags errors_;
    void (APIENTRY *finish_)();
    GlContext *context_;
};

}

#endif
