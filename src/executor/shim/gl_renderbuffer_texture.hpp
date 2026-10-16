/* A renderbuffer's storage as a 2D texture of the same share group, through
 * an EGLImage, so OpenCL can name a renderbuffer where the host's GL sharing
 * takes only textures and buffers.
 *
 * Intel's compute runtime on Linux shares through MESA_GLINTEROP and answers
 * every clCreateFromGLRenderbuffer with CL_INVALID_GL_OBJECT ("target 8d41 not
 * supported"), while clCreateFromGLTexture works. EGL_KHR_gl_renderbuffer_image
 * makes an image of the renderbuffer's own memory, and
 * glEGLImageTargetTexture2DOES a texture over it: one allocation, which GL
 * draws into as the renderbuffer and OpenCL reads as the texture - measured on
 * an Iris Xe, a later clear included.
 *
 * Every call makes the context current itself, as GlSharedBuffer's do. */
#ifndef AV_GL_RENDERBUFFER_TEXTURE_HPP
#define AV_GL_RENDERBUFFER_TEXTURE_HPP

#include "executor/shim/gl_error_flags.hpp"
#include "proto/generated/opengl/gl_types.h"

#include <stdint.h>

namespace av {

class GlContext;
class GlDisplay;

class GlRenderbufferTexture {
public:
    /* The target the texture is made as. */
    static constexpr uint32_t texture_2d = 0x0DE1u;

    GlRenderbufferTexture();
    GlRenderbufferTexture(const GlRenderbufferTexture &) = delete;
    GlRenderbufferTexture &operator=(const GlRenderbufferTexture &) = delete;

    /* Keeps `context` and `display`, which must outlive this, and resolves
     * what an image and a texture over it need. A host missing any of them
     * leaves this making nothing. */
    void configure(GlContext &context, const GlDisplay &display);
    void give_back();
    bool makes_textures() const;

    /* A texture over `renderbuffer`'s storage, named in `texture`, with the
     * image it is made through in `image`. False, with nothing made, where no
     * image of the renderbuffer can be had - no such renderbuffer, one with no
     * storage, a multisampled one - or the texture refuses it. */
    [[nodiscard]] bool make(uint32_t renderbuffer, uint32_t &texture, void *&image);
    /* The texture and its image gone; the renderbuffer stays as it was. */
    void               destroy(uint32_t texture, void *image);

private:
    GlContext *current();

    void *(*create_image_)(void *, void *, unsigned, void *, const int32_t *);
    unsigned (*destroy_image_)(void *, void *);
    void (APIENTRY *image_target_texture_)(GLenum, void *);
    void (APIENTRY *gen_textures_)(GLsizei, GLuint *);
    void (APIENTRY *bind_texture_)(GLenum, GLuint);
    void (APIENTRY *delete_textures_)(GLsizei, const GLuint *);
    GlErrorFlags errors_;
    GlContext       *context_;
    const GlDisplay *display_;
};

}  // namespace av

#endif  // AV_GL_RENDERBUFFER_TEXTURE_HPP
