/* The bytes of a GL object in the vehicle's share group, read out and written
 * back through the vehicle.
 *
 * NVIDIA's OpenCL on Linux makes no context against an EGL context - every
 * clCreateContext naming one answers CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR -
 * so where the host's OpenCL will not share, a cl_mem of its own stands in for
 * each GL object the guest opens, and cl_khr_gl_sharing's acquire and release
 * are where the bytes cross (CopiedGlSharing). This is the GL half of that:
 * a buffer's bytes, and one level of a texture packed tightly at its
 * transfer format, read and written with the pixel store at an alignment of
 * one. Nothing is left bound: a binding in the vehicle would keep an object the
 * guest deleted alive.
 *
 * Every call makes the context current itself, as GlSharedBuffer's do. */
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

    /* Keeps `context`, which must outlive this, and resolves what the copies
     * need. A host missing any of it leaves this moving nothing. */
    void configure(GlContext &context);
    void give_back();
    bool moves_bytes() const;

    /* The size of `buffer`'s store; false for a name that is no buffer. */
    [[nodiscard]] bool buffer_size(uint32_t buffer, uint64_t &bytes);
    /* The first `bytes` of `buffer`, out and in; false where GL refused. */
    [[nodiscard]] bool read_buffer(uint32_t buffer, uint64_t bytes, void *into);
    [[nodiscard]] bool write_buffer(uint32_t buffer, uint64_t bytes, const void *from);
    /* The internal format and extent of `level`; false for a name that is no
     * texture, a texture of another target, or a level it has no texels in. */
    [[nodiscard]] bool level_shape(const GlLevel &level, uint32_t &internal_format, uint32_t &width,
                                   uint32_t &height, uint32_t &depth);
    /* All of `level`'s texels, out and in, at the format, type and extent it
     * moves as; false where GL refused. */
    [[nodiscard]] bool read_level(const GlLevel &level, void *into);
    [[nodiscard]] bool write_level(const GlLevel &level, const void *from);
    /* What was written, finished: another context of the share group sees
     * it from its next command on. */
    void               finish();

private:
    GlContext *current();
    /* Whether a command since the last ask raised nothing; the errors asked
     * are cleared either way. */
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

}  // namespace av

#endif  // AV_GL_OBJECT_BYTES_HPP
